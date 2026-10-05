CROSS ?= aarch64-linux-gnu-
TOOLCHAIN ?= gcc
ZIG ?= zig
ifeq ($(TOOLCHAIN),zig)
CC := $(ZIG) cc -target aarch64-freestanding-none
OBJCOPY := $(ZIG) objcopy
CPU_FLAGS := -mcpu=cortex_a72 -mgeneral-regs-only -mstrict-align
TREE_FLAG :=
else
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
CPU_FLAGS := -mcpu=cortex-a72 -mgeneral-regs-only -mstrict-align
TREE_FLAG := -fno-tree-loop-distribute-patterns
endif
CFLAGS := -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -fno-unwind-tables -fno-asynchronous-unwind-tables $(TREE_FLAG) $(CPU_FLAGS) -Isrc -Isrc/freestanding -Ivendor/bearssl/inc $(EXTRA_CFLAGS)
# A provisioned checkout must keep Pi networking enabled on incremental builds.
# Release packaging also verifies that the actual binary embeds this identity.
ifneq ($(wildcard build/ssh_credentials_generated.h),)
CFLAGS += -DHOLLY_SSH_PROVISIONED -Ibuild
endif
LDFLAGS := -nostdlib -static -no-pie -Wl,--build-id=none -Wl,--gc-sections -T src/linker.ld
HEADERS := $(wildcard src/*.h) src/assistant.inc
OBJECTS := build/boot.o build/kernel.o build/smp.o build/mind.o build/net.o build/ethernet.o build/dma_ring.o build/reply.o build/ssh_ident.o build/tcp.o build/network_service.o build/holly.o build/lore.o build/reference.o build/documents.o build/face.o build/dashboard.o build/splash.o build/framebuffer.o build/display.o build/video.o build/clock.o build/animation.o
all: build/kernel8.img
web-test: | build
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/net.c src/client.c tests/client_test.c -o build/client_test
	./build/client_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/web_format.c tests/web_format_test.c -o build/web_format_test
	./build/web_format_test
	$(MAKE) -C vendor/bearssl -j4 > build/bearssl-host.log 2>&1
	python3 tests/https_test.py
OBJECTS += build/sha256.o build/ssh_packet.o build/ssh_kex.o build/runtime.o
OBJECTS += build/aes.o build/modexp.o build/ssh_server.o build/tcp_stream.o build/telnet.o build/http_server.o build/genet_live.o build/rng200.o build/pi_network.o
OBJECTS += build/vectors.o build/exception.o build/dhcp.o build/sdcard.o build/vault.o
OBJECTS += build/dialogue.o build/dialogue3.o build/dialogue4.o build/language_model.o build/model_store.o build/selftrain.o
OBJECTS += build/search_disk.o build/arithmetic.o build/search_cache.o build/search.o build/web_format.o build/news.o build/client.o build/https.o
BEARSSL_SOURCES := $(wildcard vendor/bearssl/src/*/*.c)
BEARSSL_OBJECTS := $(patsubst vendor/bearssl/src/%.c,build/bearssl/%.o,$(BEARSSL_SOURCES))
OBJECTS += $(BEARSSL_OBJECTS)
build/bearssl/%.o: vendor/bearssl/src/%.c
	mkdir -p $(@D)
	$(CC) $(filter-out -Werror,$(CFLAGS)) -ffunction-sections -fdata-sections -DBR_USE_UNIX_TIME=0 -DBR_USE_WIN32_TIME=0 -DBR_USE_URANDOM=0 -DBR_USE_WIN32_RAND=0 -DBR_RDRAND=0 -Ivendor/bearssl/src -c $< -o $@
SSH_SOURCES := src/search_disk.c src/arithmetic.c src/search_cache.c src/search.c src/web_format.c src/news.c src/aes.c src/modexp.c src/sha256.c src/ssh_ident.c src/ssh_kex.c src/ssh_server.c src/mind.c src/holly.c src/lore.c src/reference.c src/dialogue.c src/dialogue3.c src/dialogue4.c src/language_model.c src/model_store.c src/selftrain.c
build/ssh_host_adapter: $(SSH_SOURCES) src/documents.c tools/ssh_host_adapter.c $(HEADERS) | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) src/documents.c tools/ssh_host_adapter.c -o $@
	chmod +x $@
ssh-test: build/ssh_host_adapter
	cc -O2 -std=c11 -Wall -Wextra -Werror -shared -fPIC -Isrc src/aes.c src/modexp.c -o build/ssh_crypto_reference.so
	python3 tests/ssh_crypto_reference_test.py
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) tests/ssh_security_test.c -o build/ssh_security_test
	./build/ssh_security_test
	python3 tests/ssh_openssh_test.py
build/ssh_arm_kernel.o: tests/ssh_arm_kernel.c $(HEADERS) | build
	$(CC) $(CFLAGS) -Isrc -Ibuild/arm-test -c $< -o $@
build/ssh-arm-test.elf: build/boot.o build/smp.o build/vectors.o build/exception.o build/ssh_arm_kernel.o build/aes.o build/modexp.o build/sha256.o build/ssh_ident.o build/ssh_kex.o build/ssh_server.o build/search_cache.o build/search.o build/web_format.o build/news.o build/mind.o build/holly.o build/lore.o build/reference.o build/documents.o build/dialogue.o build/dialogue3.o build/dialogue4.o build/language_model.o build/model_store.o build/selftrain.o build/runtime.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
build/ssh-arm-test.img: build/ssh-arm-test.elf
	$(OBJCOPY) -O binary $< $@
build/https_test.o: src/https.c $(HEADERS) build/https-test/https_test_anchors.h
	$(CC) $(CFLAGS) -DHOLLY_HTTPS_TEST_ANCHORS -Ibuild/https-test -c $< -o $@
build/https_arm_kernel.o: tests/https_arm_kernel.c $(HEADERS) | build
	$(CC) $(CFLAGS) -c $< -o $@
build/https-arm-test.elf: build/boot.o build/smp.o build/vectors.o build/exception.o build/https_arm_kernel.o build/https_test.o build/web_format.o build/runtime.o $(BEARSSL_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
build/https-arm-test.img: build/https-arm-test.elf
	$(OBJCOPY) -O binary $< $@
network-test:
	cc -DHOST_TEST -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/aes.c src/dma_ring.c src/ethernet.c src/genet_live.c src/rng200.c tests/pi_network_driver_test.c -o build/pi_network_driver_test
	./build/pi_network_driver_test
	cc -O2 -std=c11 -Wall -Wextra -Werror -shared -fPIC -Isrc $(SSH_SOURCES) src/net.c src/reply.c src/tcp_stream.c tests/tcp_stream_adapter.c -o build/tcp_stream_adapter.so
	python3 tests/ssh_tcp_openssh_test.py
build:
	mkdir -p build
build/boot.o: src/boot.S | build
	$(CC) $(CFLAGS) -c $< -o $@
build/vectors.o: src/vectors.S | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: src/%.c $(HEADERS) | build
	$(CC) $(CFLAGS) -c $< -o $@
build/kernel.elf: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@
build/kernel8.img: build/kernel.elf
	$(OBJCOPY) -O binary $< $@
test:
	mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/mind.c tests/mind_test.c -o build/mind_test
	./build/mind_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/net.c tests/net_test.c -o build/net_test
	./build/net_test
	cc -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc src/ethernet.c src/dma_ring.c tests/ethernet_test.c -o build/ethernet_test
	./build/ethernet_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/dma_ring.c tests/dma_ring_test.c -o build/dma_ring_test
	./build/dma_ring_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/net.c src/reply.c tests/reply_test.c -o build/reply_test
	./build/reply_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/net.c src/reply.c src/ssh_ident.c src/tcp.c src/dma_ring.c src/network_service.c tests/network_service_test.c -o build/network_service_test
	./build/network_service_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/net.c src/ssh_ident.c src/tcp.c tests/tcp_test.c -o build/tcp_test
	./build/tcp_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/ssh_ident.c tests/ssh_ident_test.c -o build/ssh_ident_test
	./build/ssh_ident_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/search_disk.c src/arithmetic.c src/search_cache.c src/search.c src/web_format.c src/news.c src/mind.c src/holly.c src/lore.c src/reference.c src/dialogue.c src/dialogue3.c src/dialogue4.c src/language_model.c src/model_store.c src/selftrain.c tests/holly_test.c -o build/holly_test
	./build/holly_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/face.c tests/face_test.c -o build/face_test
	./build/face_test
	cc -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc src/face.c src/dashboard.c src/splash.c src/framebuffer.c tests/framebuffer_test.c -o build/framebuffer_test
	./build/framebuffer_test
	cc -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc src/face.c src/dashboard.c src/splash.c src/framebuffer.c src/display.c tests/display_test.c -o build/display_test
	./build/display_test
	cc -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc src/clock.c src/animation.c tests/timing_test.c -o build/timing_test
	./build/timing_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/sha256.c tests/crypto_test.c -o build/crypto_test
	./build/crypto_test
	cc -std=c11 -Wall -Wextra -Werror -shared -fPIC -Isrc src/sha256.c -o build/crypto_reference.so
	python3 tests/crypto_reference_test.py
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/ssh_packet.c tests/ssh_packet_test.c -o build/ssh_packet_test
	./build/ssh_packet_test
	cc -std=c11 -Wall -Wextra -Werror -Isrc src/ssh_packet.c src/ssh_kex.c tests/ssh_kex_test.c -o build/ssh_kex_test
	./build/ssh_kex_test
	cc -std=c11 -Wall -Wextra -Werror -fno-builtin -fno-tree-loop-distribute-patterns src/runtime.c tests/runtime_test.c -o build/runtime_test
	./build/runtime_test
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/net.c src/dhcp.c tests/dhcp_test.c -o build/dhcp_test
	./build/dhcp_test
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/vault.c tests/vault_test.c -o build/vault_test
	./build/vault_test
	cc -DHOST_TEST -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/sdcard.c src/vault.c tests/sdcard_test.c -o build/sdcard_test
	./build/sdcard_test
	cc -DHOST_TEST -DHOLLY_SD_TEST -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/sdcard.c tests/sdcard_startup_test.c -o build/sdcard_startup_test
	./build/sdcard_startup_test
clean:
	rm -rf build
.PHONY: all test clean

dhcp-test:
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/net.c src/dhcp.c tests/dhcp_test.c -o build/dhcp_test
	./build/dhcp_test

model-test:
	OPENBLAS_NUM_THREADS=1 python3 tests/language_model_test.py
.PHONY: model-test

selftrain-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/model_store.c tests/model_store_test.c -o build/model_store_test
	./build/model_store_test
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/dialogue.c src/dialogue3.c src/dialogue4.c src/language_model.c src/model_store.c tests/selftrain_test.c -o build/selftrain_test
	./build/selftrain_test
	OPENBLAS_NUM_THREADS=1 python3 tests/training_resume_test.py
.PHONY: selftrain-test

build/training_arm_kernel.o: tests/training_arm_kernel.c src/selftrain.c $(HEADERS) | build
	$(CC) $(CFLAGS) -c $< -o $@
build/training-arm-test.elf: build/boot.o build/smp.o build/vectors.o build/exception.o build/training_arm_kernel.o build/dialogue.o build/dialogue3.o build/dialogue4.o build/language_model.o build/model_store.o build/runtime.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
build/training-arm-test.img: build/training-arm-test.elf
	$(OBJCOPY) -O binary $< $@

# Word-level dialogue checks use the bundled original checkpoint.
dialogue-test:
	OPENBLAS_NUM_THREADS=1 python3 tests/dialogue_test.py
	OPENBLAS_NUM_THREADS=1 python3 tests/dialogue3_test.py
	OPENBLAS_NUM_THREADS=1 python3 tests/dialogue4_test.py
	OPENBLAS_NUM_THREADS=1 python3 tests/dialogue_resume_test.py

video-test: | build
	cc -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc src/face.c src/dashboard.c src/splash.c src/framebuffer.c src/display.c src/video.c tests/video_test.c -o build/video_test
	./build/video_test

.PHONY: lore-test
lore-test: | build
	python3 tools/export_lore.py
	cc -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc src/lore.c src/reference.c tests/lore_test.c -o build/lore_test
	./build/lore_test

.PHONY: access-test document-test
access-test: | build
	cc -O2 -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) src/telnet.c src/http_server.c tests/access_test.c -o build/access_test
	./build/access_test
document-test: | build
	cc -O2 -DHOST_TEST -std=c11 -Wall -Wextra -Werror -Isrc src/vault.c src/documents.c src/sha256.c src/reference.c src/lore.c tests/documents_test.c -o build/documents_test
	./build/documents_test

guest-network-test: | build
	cc -O2 -DHOLLY_GUEST_TEST -std=c11 -Wall -Wextra -Werror -shared -fPIC -Isrc $(SSH_SOURCES) src/net.c src/reply.c src/tcp_stream.c src/telnet.c src/http_server.c tests/tcp_stream_adapter.c -o build/guest_tcp_adapter.so
	python3 tests/guest_tcp_test.py

reference-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/reference.c src/lore.c src/documents.c src/sha256.c tests/reference_test.c -o build/reference_test
	./build/reference_test

.PHONY: dashboard-test conversation-test
dashboard-test: | build
	cc -DHOST_TEST -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/face.c src/dashboard.c tests/dashboard_test.c -o build/dashboard_test
	./build/dashboard_test
conversation-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) tests/conversation_test.c -o build/conversation_test
	./build/conversation_test

.PHONY: mouth-test
mouth-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/animation.c tests/mouth_test.c -o build/mouth_test
	./build/mouth_test

.PHONY: web-ui-test
web-ui-test: | build
	node tests/web_ui_test.cjs

.PHONY: reading-test
reading-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) src/documents.c src/http_server.c src/telnet.c tests/reading_test.c -o build/reading_test
	./build/reading_test

.PHONY: assistant-test
assistant-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) tests/assistant_test.c -o build/assistant_test
	./build/assistant_test

.PHONY: news-test
news-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) src/http_server.c tests/news_test.c -o build/news_test
	./build/news_test

.PHONY: search-test
search-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) src/http_server.c tests/search_test.c -o build/search_test
	./build/search_test

.PHONY: search-cache-test
search-cache-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) src/vault.c tests/search_cache_test.c -o build/search_cache_test
	./build/search_cache_test
.PHONY: arithmetic-test search-disk-test
arithmetic-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/arithmetic.c tests/arithmetic_test.c -o build/arithmetic_test
	./build/arithmetic_test
search-disk-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc src/search_disk.c tests/search_disk_test.c -o build/search_disk_test
	./build/search_disk_test
.PHONY: free-lookup-test cache-migration-test
free-lookup-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) tests/free_lookup_test.c -o build/free_lookup_test
	./build/free_lookup_test
cache-migration-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) src/vault.c src/documents.c tests/cache_migration_test.c -o build/cache_migration_test
	./build/cache_migration_test
.PHONY: always-lookup-test
always-lookup-test: | build
	cc -O2 -std=c11 -Wall -Wextra -Werror -Isrc $(SSH_SOURCES) tests/always_lookup_test.c -o build/always_lookup_test
	./build/always_lookup_test
