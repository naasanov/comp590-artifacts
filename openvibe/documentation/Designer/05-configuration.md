# Configuration Manager {#designer-configuration-manager}

\page DesignerConfiguration Configuration

In this section, we review a component common to the whole OpenViBE software platform: the **configuration manager**. We start with this component as we will mention several times in this document how specific behavior can be changed according to this configuration manager.

The OpenViBE Configuration Manager is a software component in charge of configuring OpenViBE applications and modules according to the user's wishes. Configuration settings are saved in a configuration file, which is loaded at application startup. This file uses a simple syntax, where configuration tokens are listed and given a value. You can check all the configuration tokens in Designer (Menu Edit/Preferences).

## Configuration Files

OpenViBE comes with **a default configuration file** (`share/openvibe/kernel/openvibe.conf`), automatically loaded at startup. This file lists configuration tokens used by the applications.

Users can edit **a personal configuration file** in order to customize their OpenViBE software platform, by overwriting existing token or declaring new ones. In order to do so, create a file named `openvibe-workspace.conf` in the folder next to the scenarios you are running. This file will impact all scenarios next to it.

## Syntax

A configuration file basically looks like successive `token = value` statements.

For example, the application can retrieve its root path (OpenViBE installation path) from the Path_Root variable. The path is expressed relative to the execution directory bin, so the root path is simply:

```ini
  Path_Root = ..
```

Leading and tailing spaces are allowed, and are removed automatically:

```ini
   Path_Root =       ..
```

Comments may be stored on their own lines. They begin after the `#` character:

```ini
   #this is a valid comment
   Path_Root = ..
```

Lines ending with a `\` character continue to the next line, the last `\` character of a line is automatically removed by the parser. Note that `\` is used as an escape character. To write a path, you should use `/`:

```ini
   #this is a line that extends\
    to the next line
   Path_Root = ..
```

Tokens declared anywhere in the configuration file may be used as values for other tokens. The syntax to be used is: `${token}`
For example, the binaries path may be declared as such:

```ini
   Path_Root = ..
   Path_Bin = ${Path_Root}/bin
```

Configuration files can include other configuration files thanks to a simple syntax:

```ini
   Include = path/to/my/config/file.conf
```

All tokens are read sequentially, and thus can be overwritten during the process.

## Existing Tokens

Several tokens such as *Path_Root* and *Path_Bin* are defined in the default configuration file, and can be overwritten in user-defined configuration files. The token *Player_ScenarioDirectory* is useful when designing a scenario. This token is changed as the path of the scenario when playing it. This is useful to load a file in the same folder as your current scenario. To have the complete list of these tokens, and their values, you can open the window **Preferences**.

## Defining and Using Custom Tokens

Defining your own token can be very useful as you can use them afterwards everywhere in OpenViBE. For example, you can define a token `LowPassFrequency = 40`. You can then use it as a setting in several temporal filters (`${LowPassFrequency}`). Changing the token value will automatically change the filtering frequency in every temporal filter at once, which can be very handy: modification is global and switching from one frequency to another is fast and easy.

We will describe a concrete example that illustrates the use of custom configuration tokens, in paragraph "Appendix A: Using Configuration Tokens to Setup an Experiment Environment".
