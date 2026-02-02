{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    pkg-config
    ninja
    git
  ];

  buildInputs = with pkgs; [
    gcc
    gdb
    
    vulkan-headers
    vulkan-loader
    glfw
    shaderc
    
    gsettings-desktop-schemas
    glib
    gtk3
    
    # Ultralight dependencies
    cairo
    pango
    glib
    fontconfig
    harfbuzz
    atk
    gdk-pixbuf
    xorg.libX11
    xorg.libXrandr
    xorg.libXcursor
    xorg.libXi
    xorg.libXrender
    xorg.libXext
    bzip2
    fontconfig
    freetype
  ];

  shellHook = ''  
    export LD_LIBRARY_PATH="/run/opengl-driver/lib:/run/opengl-driver-32/lib:$LD_LIBRARY_PATH"

    export WOLF_VARS="${pkgs.lib.makeLibraryPath [ 
      pkgs.vulkan-loader 
      pkgs.libGL
      pkgs.glfw 
      pkgs.bzip2 
      pkgs.fontconfig 
      pkgs.gtk3 
      pkgs.atk
      pkgs.pango
      pkgs.gdk-pixbuf
      pkgs.cairo
      pkgs.glib
    ]}:$PWD/../ThirdParty/UltraLight/linux/bin"

    # Fix for the bzip2 version mismatch
    mkdir -p ./.lib_links
    ln -sf ${pkgs.bzip2.out}/lib/libbz2.so.1 ./.lib_links/libbz2.so.1.0
    
    export LD_LIBRARY_PATH="${pkgs.renderdoc}/lib:$PWD/.lib_links:${pkgs.vulkan-validation-layers}/lib:$WOLF_VARS:$LD_LIBRARY_PATH"
    
    export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
    export VK_DRIVER_FILES="/run/opengl-driver/share/vulkan/icd.d/nvidia_icd.x86_64.json"
    
    export FONTCONFIG_FILE=${pkgs.fontconfig.out}/etc/fonts/fonts.conf
    export FONTCONFIG_PATH=${pkgs.fontconfig.out}/etc/fonts
    
    export XDG_DATA_DIRS="${pkgs.gsettings-desktop-schemas}/share/gsettings-schemas/${pkgs.gsettings-desktop-schemas.name}:${pkgs.gtk3}/share/gsettings-schemas/${pkgs.gtk3.name}:$XDG_DATA_DIRS"
    export GSETTINGS_SCHEMA_DIR="${pkgs.gtk3}/share/gsettings-schemas/${pkgs.gtk3.name}/glib-2.0/schemas"

    echo "--- Environment loaded ---"
  '';
}
