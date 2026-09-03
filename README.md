cd ~/Sisi
cat > README.md << 'EOF'
# IFDataGen - GNSS IF Data Generator

A high-performance GNSS Intermediate Frequency (IF) signal generator for GPS, Galileo, BDS, and GLONASS.

## Features

- **Multi-constellation**: GPS, Galileo, BDS, GLONASS
- **Multi-signal**: L1CA, L1C, L2C, L5 (GPS); E1, E5a, E5b (Galileo); B1I, B1C (BDS); G1, G2 (GLONASS)
- **RINEX 3.x/4.x** navigation file support
- **Dynamic trajectory**: Constant speed, acceleration, turns, vertical motion
- **Realistic modeling**: Doppler, ionospheric/tropospheric delay
- **Output formats**: IQ8, IQ16
- **OpenMP parallelization**
- **JSON configuration**

## Requirements

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential g++ gcc make cmake libomp-dev


# Clone the repository
git clone <your-repo-url>
cd Sisi/SignalSim/IFdataGen

# Build
make clean
make -j$(nproc)

# Verify
./IFdataGen -c configs/example.json