# Credits

Music by stephenpalmermail
https://soundcloud.com/stephenpalmermail

Graphic assets by Carl Olsson
https://opengameart.org/users/surt

Font by Poppy Works
https://poppyworks.itch.io/silver

# Building

Ensure you have checked out submodules e.g. via `git checkout
--recurse-submodules` or `git submodule update --init` after checkout.

Ensure you have all configuration setup, e.g. via `autoreconf` or manually via
`aclocal`, `autoconf` and `automake`:

```
# initial checkout
git checkout --recurse-submodules https://github.com/brejc8/ComPressure.git
# or if not cloned with submodules run
git submodule update --init

# set up configuration files
autoreconf --verbose --install
# or manually via
aclocal
autoconf
automake --add-missing

# configure the build or see below for how to build with Steam support enabled
./configure --disable-steam

# build in parallel, or remove the flag
make -j
```

If dependencies are missing, install them, preferably via the OS package
management, e.g. on Ubuntu / Debian-based systems:

```
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev \
  libsdl2-net-dev libsdl2-ttf-dev libxcb1-dev libzstd-dev zlib1g-dev
```

The [Steam SDK](https://partner.steamgames.com/downloads/steamworks_sdk.zip)
has to be downloaded manually.  Due to authentication you'll have to do this in
the browser and authenticate via Steam Guard.

Unpack the downloaded file `steamworks_sdk.zip` and put it in a well-known
location:

```
unzip -d <steam-sdk> steamworks_sdk.zip
ln -s <steam-sdk>/sdk/public/steam .
ln -s <steam-sdk>/sdk/redistributable_bin/linux64/libsteam_api.so .
```
