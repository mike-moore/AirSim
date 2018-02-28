./build.sh
cd ~/UnrealEngine
./GenerateProjectFiles.sh -game ~/AirSim/Unreal/Environments/Blocks/Blocks.uproject
cd ~/AirSim/Unreal/Environments/Blocks
make Blocks
