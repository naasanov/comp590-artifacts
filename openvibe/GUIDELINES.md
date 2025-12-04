# Coding Style Guide

## General rules

### Base rules

The base rules applied to this project are from the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).  
Any rule listed below takes precedence over the corresponding rule.  
For any rule/guidance not mentioned here, please refer to the base guide. 

### Naming

- User defined data types are in UpperCamelCase with the following prefixes:

  ```text
  Interface: IUpperCamelCase
  Class:     CUpperCamelCase
  Template:  TUpperCamelCase
  Struct:    SUpperCamelCase
  Enum:      EUpperCamelCase
  Union:     UUpperCamelCase
  ```

- Variables are in lowerCamelCase
- Member variables are prefix with "m_": m_lowerCamelCase
- Private methods are in lowerCamelCase()
- Public methods and functions are UpperCamelCase()
- Enumeration members are in UpperCamelCase_UnderscoreTolerant
- constants are in UPPER_CASE
- Namespaces are in UpperCamelCase

### Formatting

- Indents are using TABs
  - 4 spaces long
  - Final align with spaces if needed
- No indents inside Namespaces
- Braces follow the Allman style for **type declarations and functions**:

  ```cpp
  CNewClass
  {
    public:
      void PublicMethod();
  }

  void CNewClass::PublicMethod()
  {
      ...
  }
  ```

### Commenting

Code must be documented with doxygen comments.  
Commonly used block comments are `///` or `/** */`.  
Commonly used inline comments are `///<` or `/**< */`.  
Below is an example of different styles:

```cpp 
/**
 * \brief Class for handling file writing/reading
 */
class CFileHandling
{
  public:
    ///
    /// \brief Constructor
    /// \param fileName The path to the file to handle
    ///
    explicit CFileHandling(const std::string& fileName);
    /**
     * @brief Opens file for reading or writing
     * @return true on success, false otherwise
     */
     bool OpenFile(const std::string& fileName);

    /// <summary> Writes provided data to file </summary>
    ///
    /// <param name="data"> The data to be written </param>
    /// <param name="append"> Flag set to true to append the data, or false to overwrite the file </param>
    /// <return> true on succes, false otherwise </return>
    bool WriteToFile(const std::string& data, bool append=true);
        
  private:
    std::string m_file;  /**< The path to the file being handled */
    size_t m_fileSize;  ///< The current size of the file
}
```

Be mindful **not to mix styles** within a file or set of files.  
More infos about documenting code on [Doxygen manual](https://www.doxygen.nl/manual/docblocks.html).
