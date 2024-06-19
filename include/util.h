

#include <Preferences.h>
#include <nvs_flash.h>

void SetIniString( String sNamespace, String sKey, String sValue );

String GetIniString( String sNamespace, String sKey, String sDefaultValue );

void SetIniInt( String sNamespace, String sKey, int sValue );

int GetIniInt( String sNamespace, String sKey, int sDefaultValue );