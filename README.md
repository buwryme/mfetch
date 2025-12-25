## mfetch

MFetch (Minimal Fetch) is a system information fetcher with wide customizability in mind, based on TOML.

### How do you configure it?

Right now, the configuration of it is hardcoded to `data/mfetch.toml`, relative to the working folder the binary is running in.
MFetch renders an ASCII art besides the sysinfo block.
For an example of a MFetch configuration, you can view the `mfetch.toml` file inside the `data` folder.
Without a configuration, it sticks to... ugly defaults.

### Can I install it on my system?

... Unless you manually copy the binary AND the configuration into the folder with your binaries...no.
Package formatting is planned.

### How do I build MFetch?

Run `task build` inside the root folder of the project.

### How do I run MFetch?

Run `task run` inside the root folder of the project.

### How do I clean the build artifacts?

Run `task clean` inside the root folder of the project.

Have fun!
