// HelloWorld.c

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/JsonLib.h>

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/JsonLib.h>

EFI_STATUS
EFIAPI
CreateJsonExample (
  VOID
) {
    // Initialize a JSON object
    EDKII_JSON_OBJECT jsonObject = JsonValueInitObject();
    if (jsonObject == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Add an ASCII string
    EDKII_JSON_VALUE jsonString = JsonValueInitAsciiString("Hello, World! eSpark 123");
    if (jsonString != NULL) {
        JsonObjectSetValue(jsonObject, "greeting", jsonString);
    }

    // Add a Unicode string
    EDKII_JSON_VALUE jsonUnicodeString = JsonValueInitUnicodeString(L"EFI JSON Example");
    if (jsonUnicodeString != NULL) {
        JsonObjectSetValue(jsonObject, "unicodeGreeting", jsonUnicodeString);
    }

    // Add an integer
    EDKII_JSON_VALUE jsonInteger = JsonValueInitInteger(42);
    if (jsonInteger != NULL) {
        JsonObjectSetValue(jsonObject, "theAnswer", jsonInteger);
    }

    // Add a boolean
    EDKII_JSON_VALUE jsonBoolean = JsonValueInitBoolean(TRUE);
    if (jsonBoolean != NULL) {
        JsonObjectSetValue(jsonObject, "isTrue", jsonBoolean);
    }

    // Add a null value
    EDKII_JSON_VALUE jsonNull = JsonValueInitNull();
    if (jsonNull != NULL) {
        JsonObjectSetValue(jsonObject, "nothing", jsonNull);
    }

    // Add an array
    EDKII_JSON_ARRAY jsonArray = JsonValueInitArray();
    if (jsonArray != NULL) {
        // Append values to the array
        JsonArrayAppendValue(jsonArray, JsonValueInitInteger(1));
        JsonArrayAppendValue(jsonArray, JsonValueInitInteger(2));
        JsonArrayAppendValue(jsonArray, JsonValueInitInteger(3));
        JsonObjectSetValue(jsonObject, "numbers", jsonArray);
    }

    // Dump the JSON string to output
    CHAR8 *jsonStringDump = JsonDumpString(jsonObject, EDKII_JSON_SORT_KEYS);
    if (jsonStringDump != NULL) {
        Print(L"Generated JSON: %a\n", jsonStringDump);
        FreePool(jsonStringDump);
    }

    // Free the JSON object
    JsonValueFree(jsonObject);
    
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
) {

//   EFI_STATUS Status;
    
  CreateJsonExample();

  return EFI_SUCCESS;
}
