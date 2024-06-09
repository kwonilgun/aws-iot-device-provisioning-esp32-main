
#include "util.h"

//네임스페이스라는 그룹안에 키와 값을 저장하는 코드 입니다.
// INI 쓰기
void SetIniString( String sNamespace, String sKey, String sValue ) {
  Preferences preferences;
  preferences.begin( sNamespace.c_str(), false );
  preferences.putString( sKey.c_str(), sValue.c_str() );
  preferences.end();
}


// 네임스페이스라는 그룹안에 같은 이름의 키에 저장된 값을 읽어오는 코드 입니다.

// 만약, 존재하지 않는 키 일 때 기본값을 리턴 합니다.

// INI 읽기
String GetIniString( String sNamespace, String sKey, String sDefaultValue ) {
  Preferences preferences;
  String sValue;
  preferences.begin( sNamespace.c_str(), false );
  sValue = preferences.getString( sKey.c_str(), sDefaultValue.c_str() );
  preferences.end();
  return sValue;
}


//네임스페이스라는 그룹안에 키와 값을 저장하는 코드 입니다.
// INI 쓰기
void SetIniInt( String sNamespace, String sKey, int sValue ) {
  Preferences preferences;
  preferences.begin( sNamespace.c_str(), false );
  preferences.putInt( sKey.c_str(), sValue );
  preferences.end();
}


// 네임스페이스라는 그룹안에 같은 이름의 키에 저장된 값을 읽어오는 코드 입니다.

// 만약, 존재하지 않는 키 일 때 기본값을 리턴 합니다.

// INI 읽기
int GetIniInt( String sNamespace, String sKey, int sDefaultValue ) {
  Preferences preferences;
  int sValue;
  preferences.begin( sNamespace.c_str(), false );
  sValue = preferences.getInt( sKey.c_str(), sDefaultValue );
  preferences.end();
  return sValue;
}