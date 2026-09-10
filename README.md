# OpenWrt 25 for Lantiq Falcon SFP with Open GPON Driver

This repository builds modern **OpenWrt 25 (Linux kernel 6.12)** for **Lantiq Falcon (PEF98036)** SFP ONTs (such as Zyxel PMG3000-D20B, Nokia/Alcatel G-010S-P, Huawei MA5671A, Eltex, etc.) with a **100% clean-room open-source GPON kernel driver (`gpon-falcon`)**.

---

## Key Features

- **Modern Kernel**: Linux 6.12 (OpenWrt master / 25.x).
- **Clean-Room Open GPON Driver**:
  - Full ITU-T G.984.3 PLOAM state machine (O1..O7).
  - Hardware GTC (GPON Transmission Convergence) control.
  - GPE (Gigabit Packet Engine) microcode loader & DMA segment memory manager.
  - BOSA optical transceiver dual-loop APC (Automatic Power Control) & APD boost converter.
- **Customizable Identity**: Configure Serial Number (`onu_sn`) and Registration Password (`onu_pwd`) via module parameters or UCI.
- **Automated CI/CD**: Automatic building and image generation via GitHub Actions.

---

## Automated GitHub Actions Build

1. Push this repository to GitHub:
   ```bash
   git remote add origin https://github.com/<YOUR_USERNAME>/openwrt-falcon-gpon.git
   git push -u origin main
   ```
2. Navigate to **Actions** tab on your GitHub repository.
3. Click on **Build OpenWrt 25 for Lantiq Falcon SFP with Open GPON**.
4. Once the build completes (~25 minutes), download the firmware archive from **Artifacts**:
   - `openwrt-lantiq-falcon-falcon-sfp-squashfs-sysupgrade.bin`
   - `kmod-gpon-falcon*.ipk` / `.apk`

---

## Manual / Local Build

Run the automated Docker build script:
```bash
./build_with_docker.sh
```

---

## License

- Kernel Driver & Code: **GPL-2.0+**
- OpenWrt: **GPL-2.0**
