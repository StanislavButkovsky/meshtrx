"""Pre-build script: generate .S stubs for managed component certificates.
Workaround for pioarduino custom_sdkconfig not handling target_add_binary_data."""
import os, re, glob
Import("env")

build_dir = env.subst("$BUILD_DIR")
mc_dir = os.path.join(env.subst("$PROJECT_DIR"), "managed_components")

# Find all target_add_binary_data calls in managed components
certs = []
for cmake in glob.glob(os.path.join(mc_dir, "*/CMakeLists.txt")):
    comp_dir = os.path.dirname(cmake)
    with open(cmake) as f:
        for line in f:
            m = re.search(r'target_add_binary_data\([^"]*"([^"]+)"', line)
            if m:
                cert_rel = m.group(1)
                cert_abs = os.path.join(comp_dir, cert_rel)
                if os.path.exists(cert_abs):
                    certs.append((cert_abs, os.path.basename(cert_rel)))

for cert_path, cert_name in certs:
    s_file = os.path.join(build_dir, cert_name + ".S")
    if not os.path.exists(s_file):
        sym = re.sub(r'[^a-zA-Z0-9]', '_', cert_name)
        with open(s_file, 'w') as f:
            f.write(f""".data
.section .rodata.embedded
.global _binary_{sym}_start
.global _binary_{sym}_end
.align 4
_binary_{sym}_start:
.incbin "{cert_path}"
.byte 0
_binary_{sym}_end:
""")
        print(f"[gen_cert_stubs] Generated {s_file}")
