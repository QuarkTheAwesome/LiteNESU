# LiteNESU
## A (terrible) NES emulator for 5.5.0/5.5.1 WiiUs.

Based off [LiteNES](https://github.com/NJUOS/LiteNES). Lots of code taken from [libwiiu](https://github.com/wiiudev/libwiiu) and the [5.5.x ELF loader](https://gbatemp.net/threads/5-5-x-elf-loader.414650/).
Thanks tons. I know I didn't ask you, but your code is still awesome.
Everything that isn't mine is distributed under their respective licenses.

### Usage
You'll need devKitPPC installed and an environment variable (DEVKITPPC) set to the path to devKitPPC (not the bin folder!).
Once you have all that, just run make!

You'll end up with boot.elf in the root directory of the project. This should be loadable by the [ELF loader](https://gbatemp.net/threads/5-5-x-elf-loader.414650/) on a 5.5.x WiiU.
To have it actually work, you'll need an iNES rom called play.nes in the same directory as the ELF. It uses the same URL builder as the ELF loader. This is what worked for me:
Loader: ```http://webhost/u/payload/my_payload_file.php```
ELF: ```http://webhost/u/payload/boot.elf```
NES ROM: ```http://webhost/u/payload/play.nes```
You could probably use something different if you want.

### Caveats
This is not a replacement for Virtual Console.
If you aren't a developer, this probably won't be good enough for you.
There's no sound.
There's only one player.
It's slow. It's really, really slow. See the TODO about how I think this could be fixed.
I've only tested Yie-Ar Kung Fu (working!) and Super Mario Bros. I'm lazy, I know. Most other ROMs probably won't work.
There's some weird RAM corruption stuff going on which makes Super Mario really, really, realy bad. You'll see.

At least it's userspace!