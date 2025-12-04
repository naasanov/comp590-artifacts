# Introduction {#designer-introduction}

\page DesignerIntroduction Introduction

The OpenViBE Designer is an authoring tool dedicated to creating and executing OpenViBE scenarios. It is targeted at a broad range of users, including:

- **researchers** and students of the neuroscience and BCI community
- **neurophysiology experts** who need a tool to realize signal-processing and monitoring of EEG activity
- **clinicians** looking for a tool to conduct neurofeedback experiments
  
It relies on a graphical user interface to provide signal processing tools in an intuitive way, and doesn’t require any programming skills.

Each of these tools comes as a plugin, which communicates with the application via a generic interface hiding implementation details. As a result, it is easy for a programmer to extend the range of tools provided with the platform. Users may arrange any number of these boxes in a very flexible fashion, considering there is virtually no limit as to the number of boxes that may be included in a processing **scenario**.

Once a scenario is created, it may be run from Studio, which provides a toolbar for playing, pausing and stepping through a scenario. A number of box algorithms are available for direct **visualization** of results, from simple 2D displays such as Spectral Analysis and Continuous Oscilloscope to 3D paradigms such as 3D Topography. The layout of these displays may be customized as desired at scenario editing time using the **window manager** module of Studio.
