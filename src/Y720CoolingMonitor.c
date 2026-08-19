#define _WIN32_DCOM

#include "Y720CoolingMonitor.h"

#include <windows.h>
#include <wbemidl.h>
#include <oleauto.h>


#define Y720_NAMESPACE \
    L"ROOT\\WMI"

#define Y720_OBJECT_PATH \
    L"LENOVO_GAMEZONE_DATA.InstanceName=\"ACPI\\\\PNP0C14\\\\GMZN_0\""


static IWbemLocator *g_locator = NULL;
static IWbemServices *g_services = NULL;
static BSTR g_object_path = NULL;

static int g_com_initialized = 0;


/* ---------------------------------------------------------
   Read numeric VARIANT into LONG
   --------------------------------------------------------- */

static int variant_to_long(
    VARIANT *value,
    LONG *result)
{
    if (!value || !result)
        return -1;


    switch (V_VT(value))
    {
        case VT_UI1:

            *result =
                (LONG)V_UI1(value);

            return 0;


        case VT_UI2:

            *result =
                (LONG)V_UI2(value);

            return 0;


        case VT_UI4:

            *result =
                (LONG)V_UI4(value);

            return 0;


        case VT_I1:

            *result =
                (LONG)V_I1(value);

            return 0;


        case VT_I2:

            *result =
                (LONG)V_I2(value);

            return 0;


        case VT_I4:

            *result =
                V_I4(value);

            return 0;


        default:

            return -1;
    }
}


/* ---------------------------------------------------------
   Read one GameZone WMI method
   --------------------------------------------------------- */

static int read_method(
    const wchar_t *method_name,
    WMI_RESULT *result)
{
    HRESULT hr;

    BSTR method = NULL;

    IWbemClassObject *output = NULL;

    VARIANT value;


    if (!method_name || !result)
        return -1;


    result->data = 0;
    result->return_value = -1;
    result->success = 0;


    if (!g_services || !g_object_path)
        return -1;


    VariantInit(&value);


    method =
        SysAllocString(
            method_name
        );

    if (!method)
        return -1;


    /*
     * Execute the read-only GameZone method.
     */
    hr =
        g_services->lpVtbl->ExecMethod(
            g_services,
            g_object_path,
            method,
            0,
            NULL,
            NULL,
            &output,
            NULL
        );


    SysFreeString(method);


    if (FAILED(hr))
        return -1;


    if (!output)
        return -1;


    /*
     * Lenovo puts the useful result in Data.
     */
    hr =
        output->lpVtbl->Get(
            output,
            L"Data",
            0,
            &value,
            NULL,
            NULL
        );


    if (FAILED(hr))
    {
        output->lpVtbl->Release(output);
        return -1;
    }


    if (variant_to_long(
            &value,
            &result->data) != 0)
    {
        VariantClear(&value);

        output->lpVtbl->Release(output);

        return -1;
    }


    VariantClear(&value);


    /*
     * ReturnValue is optional.
     */
    VariantInit(&value);


    hr =
        output->lpVtbl->Get(
            output,
            L"ReturnValue",
            0,
            &value,
            NULL,
            NULL
        );


    if (SUCCEEDED(hr))
    {
        LONG rv;


        if (variant_to_long(
                &value,
                &rv) == 0)
        {
            result->return_value = rv;
        }
    }


    VariantClear(&value);


    output->lpVtbl->Release(output);


    result->success = 1;


    return 0;
}


/* ---------------------------------------------------------
   Initialize Lenovo GameZone WMI
   --------------------------------------------------------- */

int GameZoneInitialize(void)
{
    HRESULT hr;

    BSTR namespace_name = NULL;


    if (g_services)
        return 1;


    /*
     * Initialize COM.
     */
    hr =
        CoInitializeEx(
            NULL,
            COINIT_MULTITHREADED
        );


    if (FAILED(hr))
        return 0;


    g_com_initialized = 1;


    /*
     * Initialize COM security.
     */
    hr =
        CoInitializeSecurity(
            NULL,
            -1,
            NULL,
            NULL,
            RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL,
            EOAC_NONE,
            NULL
        );


    if (FAILED(hr) &&
        hr != RPC_E_TOO_LATE)
    {
        GameZoneShutdown();
        return 0;
    }


    /*
     * Create WMI locator.
     */
    hr =
        CoCreateInstance(
            &CLSID_WbemLocator,
            NULL,
            CLSCTX_INPROC_SERVER,
            &IID_IWbemLocator,
            (void **)&g_locator
        );


    if (FAILED(hr))
    {
        GameZoneShutdown();
        return 0;
    }


    /*
     * Connect to ROOT\WMI.
     */
    namespace_name =
        SysAllocString(
            Y720_NAMESPACE
        );


    if (!namespace_name)
    {
        GameZoneShutdown();
        return 0;
    }


    hr =
        g_locator->lpVtbl->ConnectServer(
            g_locator,
            namespace_name,
            NULL,
            NULL,
            NULL,
            0,
            NULL,
            NULL,
            &g_services
        );


    SysFreeString(namespace_name);


    if (FAILED(hr))
    {
        GameZoneShutdown();
        return 0;
    }


    /*
     * Configure WMI proxy.
     */
    hr =
        CoSetProxyBlanket(
            (IUnknown *)g_services,
            RPC_C_AUTHN_WINNT,
            RPC_C_AUTHZ_NONE,
            NULL,
            RPC_C_AUTHN_LEVEL_CALL,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL,
            EOAC_NONE
        );


    if (FAILED(hr))
    {
        GameZoneShutdown();
        return 0;
    }


    /*
     * Known working Lenovo Y720 GameZone object.
     */
    g_object_path =
        SysAllocString(
            Y720_OBJECT_PATH
        );


    if (!g_object_path)
    {
        GameZoneShutdown();
        return 0;
    }


    return 1;
}


/* ---------------------------------------------------------
   Read v1.0 telemetry
   --------------------------------------------------------- */

int GameZoneRead(
    GAMEZONE_DATA *data)
{
    if (!data)
        return 0;


    if (!g_services ||
        !g_object_path)
    {
        return 0;
    }


    ZeroMemory(
        data,
        sizeof(*data)
    );


    /*
     * Fan 1 RPM.
     */
    read_method(
        L"GetFan1Speed",
        &data->fan1
    );


    /*
     * Fan 2 RPM.
     */
    read_method(
        L"GetFan2Speed",
        &data->fan2
    );


    /*
     * Lenovo IR / thermal sensor.
     */
    read_method(
        L"GetIRTemp",
        &data->ir_temp
    );


    return 1;
}


/* ---------------------------------------------------------
   Set Lenovo Extreme Cooling
   ---------------------------------------------------------

   Lenovo WMI method:

       SetFanCooling(Data)

   The WMI class describes Data as CIM_UINT32,
   but the working diagnostic implementation on
   this machine accepted VT_I4.

   Therefore we intentionally send VT_I4.

   setting = 1 -> ON
   setting = 0 -> OFF

   This is the only write operation in the library.
   --------------------------------------------------------- */

BOOL GameZoneSetFanCooling(
    DWORD setting)
{
    HRESULT hr;

    BSTR class_name = NULL;
    BSTR method_name = NULL;

    IWbemClassObject *class_object = NULL;
    IWbemClassObject *method_in_class = NULL;
    IWbemClassObject *input_params = NULL;
    IWbemClassObject *output_params = NULL;

    VARIANT value;

    BOOL success = FALSE;


    if (!g_services ||
        !g_object_path)
    {
        return FALSE;
    }


    /*
     * Only 0 and 1 are valid.
     */
    if (setting != 0 &&
        setting != 1)
    {
        return FALSE;
    }


    VariantInit(&value);


    /*
     * Get the Lenovo GameZone class.
     */
    class_name =
        SysAllocString(
            L"LENOVO_GAMEZONE_DATA"
        );


    if (!class_name)
        goto cleanup;


    hr =
        g_services->lpVtbl->GetObject(
            g_services,
            class_name,
            0,
            NULL,
            &class_object,
            NULL
        );


    if (FAILED(hr))
        goto cleanup;


    /*
     * Get SetFanCooling method definition.
     */
    method_name =
        SysAllocString(
            L"SetFanCooling"
        );


    if (!method_name)
        goto cleanup;


    hr =
        class_object->lpVtbl->GetMethod(
            class_object,
            method_name,
            0,
            &method_in_class,
            NULL
        );


    if (FAILED(hr))
        goto cleanup;


    /*
     * Create method input parameter object.
     */
    hr =
        method_in_class->lpVtbl->SpawnInstance(
            method_in_class,
            0,
            &input_params
        );


    if (FAILED(hr))
        goto cleanup;


    /*
     * IMPORTANT:
     *
     * The working diagnostic program showed that
     * VT_I4 is accepted by this Lenovo implementation.
     */
    V_VT(&value) = VT_I4;
    V_I4(&value) = (LONG)setting;


    hr =
        input_params->lpVtbl->Put(
            input_params,
            L"Data",
            0,
            &value,
            0
        );


    VariantClear(&value);


    if (FAILED(hr))
        goto cleanup;


    /*
     * Execute SetFanCooling.
     *
     * setting = 1 -> Extreme Cooling ON
     * setting = 0 -> Extreme Cooling OFF
     */
    hr =
        g_services->lpVtbl->ExecMethod(
            g_services,
            g_object_path,
            method_name,
            0,
            NULL,
            input_params,
            &output_params,
            NULL
        );


    if (FAILED(hr))
        goto cleanup;


    /*
     * IMPORTANT:
     *
     * Lenovo may return Data=0 / ReturnValue=-1
     * even when the physical command succeeds.
     *
     * Therefore HRESULT from ExecMethod is used
     * to determine whether the command succeeded.
     */
    success = TRUE;


cleanup:

    VariantClear(&value);


    if (output_params)
    {
        output_params->lpVtbl->Release(
            output_params
        );
    }


    if (input_params)
    {
        input_params->lpVtbl->Release(
            input_params
        );
    }


    if (method_in_class)
    {
        method_in_class->lpVtbl->Release(
            method_in_class
        );
    }


    if (class_object)
    {
        class_object->lpVtbl->Release(
            class_object
        );
    }


    if (method_name)
    {
        SysFreeString(
            method_name
        );
    }


    if (class_name)
    {
        SysFreeString(
            class_name
        );
    }


    return success;
}


/* ---------------------------------------------------------
   Shut down GameZone WMI
   --------------------------------------------------------- */

void GameZoneShutdown(void)
{
    if (g_object_path)
    {
        SysFreeString(
            g_object_path
        );

        g_object_path = NULL;
    }


    if (g_services)
    {
        g_services->lpVtbl->Release(
            g_services
        );

        g_services = NULL;
    }


    if (g_locator)
    {
        g_locator->lpVtbl->Release(
            g_locator
        );

        g_locator = NULL;
    }


    if (g_com_initialized)
    {
        CoUninitialize();

        g_com_initialized = 0;
    }
}