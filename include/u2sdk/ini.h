#pragma once
// Tiny INI reader (Win32 GetPrivateProfile*). Default file: scripts\<ModuleName>.ini
#include <string>

namespace u2 {
class Ini {
 public:
  Ini();                                   // <module dir>\<module name>.ini
  int GetInt(const char* section, const char* key, int def) const;
  float GetFloat(const char* section, const char* key, float def) const;
  bool GetBool(const char* section, const char* key, bool def) const { return GetInt(section, key, def) != 0; }
  std::string GetString(const char* section, const char* key, const char* def) const;
  const std::string& Path() const { return path_; }

 private:
  std::string path_;
};

// Parses a virtual-key name such as "TAB", "F7", "SPACE", "A", "0x09". Returns 0 if unknown.
int ParseVirtualKey(const std::string& name);
}  // namespace u2
