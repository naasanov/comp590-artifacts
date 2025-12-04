# OpenViBE File Format {#file-format}

\page FileFormat OpenViBE File Format

## Scenarios and Metaboxes : `.xml` (`.mxs` and `.mxb` for old mensia files)

Scenarios are used to define the set of consecutive actions to be performed for your *trial*, they are [xml](https://en.wikipedia.org/wiki/XML) files. It looks like this:

```xml
<OpenViBE-Scenario>
	<FormatVersion>2</FormatVersion>		<!-- Version of the xml scenario format unchanged since version 2.0.0 -->
	<Creator>OpenViBE Designer</Creator>	<!-- Currently the only scenario editor -->
	<CreatorVersion>X.X.X</CreatorVersion>	<!-- The version of designer on the last save  -->
	<Settings>...</Settings>				<!-- General settings of scenario -->
	<Inputs>...</Inputs>					<!-- Inputs of scenario used for Metaboxes -->
	<Outputs>...</Outputs>					<!-- Outputs of scenario used for Metaboxes -->
	<Boxes>...</Boxes>						<!-- Boxes on your scenarios -->
	<Links>...</Links>						<!-- Links between boxes on your scenario -->
	<Comments>...</Comments>				<!-- Comments (post-it) on your scenario -->
	<Metadata>...</Metadata>				<!-- Metadata of your scenario is currently only for Display manager -->
	<Attributes>...</Attributes>			<!-- Attributes (author, brief, version...) of your scenario -->
</OpenViBE-Scenario>
```

### Settings Format

The settings appear in the designer's Scenario Configuration tab. They allow the experimenter to create variables that can be used in various boxes (One variable = one `<Setting>` tag). This way, if different boxes share some parameters values, the experiementer can create a variable and use it in all these boxes. Thus, when they want to change a shared parameter, they only have to change the value of the variable instead of changing the parameters of all the boxes one by one. 

```xml
<Setting>
	<Identifier>...</Identifier>			<!-- Random ID -->
	<TypeIdentifier>...</TypeIdentifier>	<!-- Id of setting type (string, filename, stimulation....) -->
	<Name>...</Name>						<!-- Name of your Setting (you can use it with $var{Your-Name} in boxes) -->
	<DefaultValue>...</DefaultValue>		<!-- Obvious -->
	<Value>...</Value>						<!-- Obvious -->
</Setting>
```

### Inputs/Outputs Format

This parts are used to create Inputs/Outputs for your metaboxes.

```xml
<Output>
	<Identifier>...</Identifier>						<!-- Random ID -->
	<TypeIdentifier>...</TypeIdentifier>				<!-- Id of stream type (signal, matrix, stimulations....) -->
	<Name>...</Name>									<!-- Obvious -->
	<LinkedBoxIdentifier>...</LinkedBoxIdentifier>		<!-- ID Of the Box linked -->
	<LinkedBoxOutputIndex>...</LinkedBoxOutputIndex>	<!-- Position of Input/Output in the Box -->
</Output>
```

### Boxes Format

```xml
<Box>
	<Identifier>...</Identifier>								<!-- Random ID -->
	<Name>...</Name>											<!-- Obvious -->
	<AlgorithmClassIdentifier>...</AlgorithmClassIdentifier>	<!-- ID Of Box algorithm -->
	<Inputs>...</Inputs>										<!-- List of Box Inputs with for each Id of stream type (signal, matrix, stimulations....) and name. -->
	<Outputs>...</Outputs>										<!-- List of Box Outputs with for each Id of stream type (signal, matrix, stimulations....) and name. -->
	<Settings>...</Settings>									<!--Same as general settings -->
	<Attributes>...</Attributes>								<!--Attributes are, position x/y, Box flag, Initial hash value (for updates), inputs/outputs/settings number  -->
</Box>
```

### Links Format

```xml
<Link>
	<Identifier>...</Identifier>				<!-- Random ID -->
	<Source>
		<BoxIdentifier>...</BoxIdentifier>		<!-- ID Of the Box linked -->
		<BoxOutputIndex>...</BoxOutputIndex>	<!-- Position of Input/Output in the Box -->
	</Source>
	<Target>
		<BoxIdentifier>...</BoxIdentifier>		<!-- ID Of the Box linked -->
		<BoxInputIndex>...</BoxInputIndex>		<!-- Position of Input/Output in the Box -->
	</Target>
</Link>
```

### Comments Format

```xml
<Comment>
	<Identifier>...</Identifier>	<!-- Random ID -->
	<Text>...</Text>				<!-- Text of your comment -->
	<Attributes>...</Attributes>	<!-- Attributes are, position x/y -->
</Comment>
```

## Config : `.xml` (`.cfg` or any extension)

Config files are used to bypass the settings of a box, they are [xml](https://en.wikipedia.org/wiki/XML) files. They all have a similar format as follows:

```xml
<OpenViBE-SettingsOverride>
	<SettingValue>First Setting Value</SettingValue>
	<SettingValue>Second Setting Value</SettingValue>
	.....
	<SettingValue>Last Setting Value</SettingValue>
</OpenViBE-SettingsOverride>
```

## Electrode Localisation : `.txt`

The electrode localisation file is used to specify the position of the electrodes in a (virtual) space. This is a simple text file as follows:

**For  Cartesian position:**

```text
[
	["Electrode Name 1" "Electrode Name 2" ..... "Last Electrode Name"]
	["x" "y" "z"]
]

[ [ x y1 z1 ] ]
[ [ x2 y2 z2 ] ]
...
[ [ LastX LastY LastZ ] ]
```

See for example [sdk\plugins\processing\file-io\electrode_sets\electrode_set_standard_cartesian.txt](https://gitlab.inria.fr/openvibe/sdk/-/blob/master/plugins/processing/file-io/electrode_sets/electrode_set_standard_cartesian.txt)

**For Spherical position:**

```text
[
	["Electrode Name 1" "Electrode Name 2" ..... "Last Electrode Name"]
	["theta" "phi"]
]

[ [ theta1 phi1 ] ]
[ [ theta2 phi2 ] ]
...
[ [ LastTheta lastPhi ] ]
```

See for example [sdk\plugins\processing\file-io\electrode_sets\electrode_set_standard_spherical.txt](https://gitlab.inria.fr/openvibe/sdk/-/blob/master/plugins/processing/file-io/electrode_sets/electrode_set_standard_spherical.txt)

## Keyboard Stimulation : `.txt`

The Keyboard Stimulation file is used to specify stimulation associated to key pressed and released. We indicate the `Key`, the stimulation when we `press` the key and the stimulation when we `release` the key. This is a simple text file as follows:

```text
a	0x00008101	0x00008100
z	0x00008102	0x00008100
e	0x00008103	0x00008100
r	0x00008104	0x00008100
t	0x00008105	0x00008100
y	0x00008106	0x00008100
```

## Signal Files

### OpenViBE : `.ov`

[OpenViBE](http://openvibe.inria.fr/) has its own file extension, `.ov`, which uses the EBML format. It is a Binay file format, meaning it's not readable by simple text editors.

**Streams** : OV contains any stream you want : Experiment informations, signal, matrix, stimulations.... possibly several stream of the same type.  
**Header** : Header contains definition of each stream.  
**Signal** : Streams are saved after header and are updated on the file at each input. Independant of stream type.

### Comma-Separated Values : `.csv`

[csv](https://en.wikipedia.org/wiki/Comma-separated_values) is the only *Raw text* format for signal, this format is explicit, it's a table with comma (at origin) to separate values. Other separator are possible for example microsoft excel consider `;` as separator.  

**Streams** : CSV contains 1 or 2 streams : Signal stream (optionnal) and stimulations.  
**Header** : Header is columns with time, channels, event (stimulations).  
**Signal** : Signal is saved after header and is update on file at each input.  
**Stimulations** :  Stimulations are saved after Signal on same line. If several stimulations are present they are separated by `:`.

For Openvibe, we have 5 possible definitions for signal files easily identifiable by the number and the name of the columns :

- Signal, example with 4 Channels  
`Time:512Hz,Epoch,C1,C2,C3,C4,Event Id,Event Date,Event Duration`
- Feature, example with 4 Features  
`Time:4,End Time,Feature 1,Feature 2,Feature 3,Feature 4,Event Id,Event Date,Event Duration`
- Spectrum, example with a signal of 128Hz discretized in 3 sections compared to the first 3 (so all here)
`Time:3x3:64,End Time,0:0.000000,0:16.000000,0:32.000000,16:0.000000,16:16.000000,16:32.000000,32:0.000000,32:16.000000,32:32.000000,Event Id,Event Date,Event Duration`
- Matrix, example with a 2x4 matrix (2 channels and 4 sample per channel)  
`Time:2x4,End Time,C1:,C1:,C1:,C1:,C2:,C2:,C2:,C2:,Event Id,Event Date,Event Duration`
- Stimulation  
`Event Id,Event Date,Event Duration`

### General Data Format for Biomedical Signals : `.gdf`

[General Data Format](https://en.wikipedia.org/wiki/General_Data_Format_for_Biomedical_Signals) is a Binay file format so you can't open file with a simple text editor.

**Streams** : GDF contains 3 streams : Experiment informations, signal and stimulations  
**Header** : Header contains Experiment informations, channel names and min/max of this channel  
**Experiment Info** : Experiment Info is saved in the Header.  
**Signal** : Signal is saved after header and is update on file at each input, during this step we update header min/max for each channel  
**Stimulations** :  Stimulations are saved after Signal when the scenario stops normally, so if OpenViBE crashes before that, no stimulation is stored in the file.

### European Data Format : `.edf`

[European Data Format](https://en.wikipedia.org/wiki/European_Data_Format) is a Binay file format so you can't open file with a simple text editor.

EDF is similar to GDF, but the header only contains the Experiment Information. After the header, there are the stimulations (updated at each input). The signal is only saved when the scenario stops normally, so if OpenViBE crashes before that, no signal is stored in the file.

### BrainVision : `.vhdr`, `.eeg`, `.vmrk`

[BrainVision](https://brainvision.com/) has a Binay file format so you can't open file with a simple text editor. It's saved in 3 files : Header in `.vhdr`, signal in `.eeg` and stimulation in `.vmrk`.

**Streams** : BrainVision format contains 2 streams : signal and stimulations.  
**Header** : Header contains channel names.  
**Signal** : Signal is saved without format, each data is saved consecutivly.  
**Stimulations** :  Stimulations are saved one stimulation per lines with Name of stimulation or id (if doesn't exist), time and duration.

### BCI2000 : `.dat`

[BCI2000](https://www.bci2000.org/mediawiki/index.php/Main_Page) has a Binay file format so you can't open file with a simple text editor. OpenViBE can only read this data not write.

**Streams** : BCI2000 format contains 2 streams : signal and "states" equivalent of stimuations.  
**Header** : Header contains only channel number and sampling rate.  
**Signal** : Signal (and state) is saved after header, we must know the sample per buffer previously to have a valid signal. We compute time of signal with sampling rate and number of samples sent.
