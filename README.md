# Getting the Gain Value from a SiPM using an LED Pulsar with CAEN DT5202
This code creates a basis for obtaining a gain value for an individual SiPM from an LED pulsar in the setup currently in use at Wright Lab in Dr. Caines' lab. The data used in the code is readout by a CAEN DT5202 in a txt format; it is turned into a histogram in ROOT where the background is subtracted and the resulting data is fit to Gaussians to get the gain. 

The goal of this code is to extract the gain value.

## Overview 
Silicon Photomultipliers are going to be used in the LFHCal in the EIC being built at BNL. However, the data that they read out is read out as Analog-to-Digitcal Converter (ADC) units. These units are arbitrary and as such, need to be scaled into a unit that is recognizable by the general scientific community. This is where the gain of the SiPM comes in. The gain value is the distance between n photons in a SiPM, so if one can divide the ADC by the gain, one can turn the ADC into PE, or photoelectrons. To calculate this gain value, one bombards the SiPM with photons to create a Single-Photon-Spectrum (SPS) graph. This SPS graph has a lot of noise. To account for this, a Landua-Gauss convolution fit is used on the troughs of the graph to subtract the background away. The resulting data set has individual peaks that can be fit to Gaussian distributions to get the mean value. These mean values are then subtracted from one another to get the gain, defined as the difference between 2 peaks next to each other. Since there are multiple peaks, this gain value is then averaged among all values to get the gain used. 

## Features 
- Prompts user with statements to ensure correct file path and setup
- Reads in all data from the directory
- Stores data locally in vectors to be used in a variety of graphs
- Currently creates two graphs via ROOT to display both subtracted and non-subtracted data

## Dependencies 
This code requires:

- C++17 or later
    - Uses the following standard libraries: iostream, fstream, filesystem, vector, algorithm, tuple
- ROOT 6
    - Required for histogramming, graphing, fitting, and visualization.
- A C++ compiler with C++17 support
    - Examples:
    - GCC 8+
    - Clang 7+
- Text input data files
    - .txt files exported from the CAEN DT-5202 digitizer (in the standard column format).

## Building and Executing 
To run inside of ROOT:

## Example ROOT SPS Graph

```bash
.L SPS_Gain.C
main()
```
and then follow prompts
