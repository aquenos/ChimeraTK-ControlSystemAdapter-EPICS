ChimeraTK Device Access Example IOC
===================================

This EPICS IOC serves as an example of how to create an IOC that talks to a
ChimeraTK Device Access device. In this example, we simply use a dummy device,
that only stores register contents in memory. A real-world IOC would probably
rather talk to a hardware device (e.g. a PCIe device).

Due to the pluggable nature of ChimeraTK Device Access, the changes needed to
talk to an actual hardware device are minimal: You only have to exchange the
`.dmap` and `.map` files and you are ready to go.

When creating your own IOC, you should not simply copy this IOC. You should
rather use it as an example of how to create your own IOC. In the following, we
are going through the steps needed to create such an IOC. Please refer to the
individual files named here in order to learn more about the details.

1. Create an IOC using `makeBaseApp.pl -t ioc`.
2. Create the IOC startup scripts using `makeBaseApp.pl -t ioc -i`.
3. *Optionally*, adapt the startup script to use a common script for all
   platforms (like this example IOC does). However, this is not required and if
   you do not want to support multiple platforms, you can skip this step.
4. Edit `configure/CONFIG_SITE`. Copy the sections needed for applying the
   correct linker flags from this example IOC.
5. *Optionally*, create a file `configure/CONFIG_SITE.local` where you set the
   to ChimeraTK Device Access and the Control System Adapter. This is only
   needed if they are not installed in one of the system's default locations.
6. Edit `configure/RELEASE`. Add a reference to the ChimeraTK device support
   (see the file in this example IOC for an example of how to do this).
7. Edit `yourApp/src/Makefile` (the path `yourApp` obviously depends on the name
   that you used when creating the IOC). Add the ChimeraTK's device support's
   library and `.dbd` file to the list of dependencies (see the file in this
   example IOC for an example of how to do this).
8. Create a `.db` file in `yourApp/Db` and include it in `yourApp/Db/Makefile`.
   This example IOC contains an example `.db` file that you can use as a
   reference.
9. Edit `iocBoot/*/st.cmd`. Add the lines for setting the path to the `.dmap`
   file, opening the device and loading your `.db` file. You can use the file
   `iocBoot/common/st.cmd` from this example IOC as a reference.
