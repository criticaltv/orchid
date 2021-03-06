# Cycc/Cympile - Shared Build Scripts for Make
# Copyright (C) 2013-2019  Jay Freeman (saurik)

# Zero Clause BSD license {{{
#
# Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# }}}


export PATH := $(CURDIR)/$(pwd)/path:$(PATH)

.PHONY:
all:

uname-s := $(shell uname -s)
uname-o := $(shell uname -o 2>/dev/null)
-include $(pwd)/uname-$(uname-s).mk

version := $(shell $(pwd)/version.sh)
monotonic := $(word 1,$(version))
revision := $(word 2,$(version))
package := $(word 3,$(version))
version := $(word 4,$(version))

export SOURCE_DATE_EPOCH := $(monotonic)

archs := 
each = $(call loop,_,$(archs))

cflags := 
qflags := 
lflags := 
wflags := 
xflags := 

source := 
linked := 
header := 
sysroot := 

output := out-$(target)
archive := 

qflags += -gfull -Os

qflags += -fdebug-prefix-map=./=
qflags += -fdebug-prefix-map=$(CURDIR)=.

cflags += -Wall
cflags += -Werror

cflags += -fmessage-length=0
cflags += -ferror-limit=0
cflags += -ftemplate-backtrace-limit=0

beta := false

include ../default.mk
-include ../identity.mk

ifeq ($(filter nostrip,$(debug)),)
lflags += -Wl,-s
endif

objc := false
include $(pwd)/target-$(target).mk

cflags += -I@/usr/include

define _
cflags/$(1) := -I$(output)/$(1)/usr/include
cc/$(1) := $(cc) $(more/$(1))
cxx/$(1) := $(cxx) $(more/$(1))
objc/$(1) := $(objc) $(more/$(1))
endef
$(each)

define depend
$(foreach arch,$(archs),$(eval $(output)/$(arch)/$(1): $(patsubst @/%,$(output)/$(arch)/%,$(2))))
endef
$(each)

define preamble
$(eval temp := $(subst /,$(space),$*))
$(eval arch := $(firstword $(temp)))
$(eval folder := $(subst $(space),/,$(wordlist 2,$(words $(temp)),$(temp))))
endef
specific = $(eval $(value preamble))
