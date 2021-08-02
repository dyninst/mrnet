--[[

    Module cray-mrnet

    (C) COPYRIGHT CRAY INC.
    UNPUBLISHED PROPRIETARY INFORMATION.
    ALL RIGHTS RESERVED.

]]--

-- local vars: define & assign --

-- template variables ----------------------------------------------------------
local INSTALL_ROOT       = "[@%PREFIX_PATH%@]"
local MOD_LEVEL          = "[@%MODULE_VERSION%@]"
--------------------------------------------------------------------------------

local NICKNAME  = "mrnet"
local PE_DIR    = INSTALL_ROOT .. "/" .. NICKNAME .. "/" .. MOD_LEVEL

 -- module release info variables
local REL_FILE            = PE_DIR .. "/release_info"
local rel_info            = ""
if isFile(REL_FILE) then
    local f = io.open(REL_FILE, "r")
    local data = f:read("*all")
    f:close()
    if data ~= nil then rel_info = data end
end

 -- standered Lmod functions --

help ([[

The modulefile defines the system paths and
variables for the product cray-mrnet.

]] .. rel_info .. "\n" .. [[

===================================================================
To re-display ]] .. tostring(myModuleName()) .. "/" .. MOD_LEVEL .. [[ release information,
type:    less ]] .. REL_FILE .. "\n" .. [[
===================================================================

]])

whatis("Loads the MRNET - Multicast Reduction Network modulefile.")

 -- environment modifications --

setenv (           "MRNET_LEVEL",              MOD_LEVEL                   )
setenv (           "MRNET_BASEDIR",            PE_DIR                      )
setenv (           "PE_MRNET_MODULE_NAME",     myModuleName()              )
setenv (           "MRNET_VERSION",            MOD_LEVEL                   )
setenv (           "MRNET_INSTALL_DIR",        PE_DIR                      )

append_path   (    "PE_PRODUCT_LIST",            "MRNET"                   )

prepend_path  (    "LD_LIBRARY_PATH",            PE_DIR .. "/lib"            )
prepend_path  (    "PKG_CONFIG_PATH",            PE_DIR .. "/lib/pkgconfig"  )
prepend_path  (    "PE_PKG_CONFIG_PATH",         PE_DIR .. "/lib/pkgconfig"  )
prepend_path  (    "PE_PKGCONFIG_PRODUCTS",      "PE_MRNET"                  )
