## Environment Setup
_Note: Much of this is repeated from the [OpenViBE setup instructions](https://gitlab.inria.fr/openvibe/meta). Refer to those further if complications arise, especially for Windows_
1. install miniconda: https://docs.conda.io/projects/miniconda/en/latest/
2. Clone this repository `git clone https://github.com/naasanov/comp590-artifacts.git`
3. Install openvibe dependencies: `conda env update -f conda/env_{linux|osx|windows}.yaml`
4. Activate the environment: `conda activate openvibe`
5. Add additional dependencies: `conda install -c conda-forge gtk2 glib onnxruntime-cpp`
6. Make build dir: `cd openvibe; mkdir build; cd build`
7. Configure and build: `cmake .. && make -j4`
8. Launch: `./bin/openvibe-designer-3.6.0`

_This build process is fairly unstable, especially on modern systems. I had to switch from Windows 11 to WSL and still faced issues. Expect to tackle a couple small issues manually to get it fully built._
