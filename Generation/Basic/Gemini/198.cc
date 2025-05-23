#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleV2VWaveSimulation");

static void
PrintStats(Ptr<UdpClient> client, Ptr<UdpServer> server)
{
    NS_LOG_INFO("Client sent " << client->GetTxPackets() << " packets.");
    NS_LOG_INFO("Server received " << server->GetRxPackets() << " packets.");
    std::cout << "Client sent " << client->GetTxPackets() << " packets." << std::endl;
    std::cout << "Server received " << server->GetRxPackets() << " packets." << std::endl;
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("SimpleV2VWaveSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("WaveNetDevice", LOG_LEVEL_DEBUG);
    // LogComponentEnable("WifiPhy", LOG_LEVEL_DEBUG);

    double simulationTime = 10.0; // seconds

    // 1. Create two nodes representing vehicles
    NodeContainer vehicles;
    vehicles.Create(2);

    // 2. Install mobility models (constant position for simplicity)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // Vehicle 0 at origin
    positionAlloc->Add(Vector(100.0, 0.0, 0.0)); // Vehicle 1 at 100m along X-axis
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(vehicles);

    // 3. Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // 4. Configure Wave (802.11p) devices
    WaveHelper waveHelper;

    // Configure 802.11p specific parameters for PHY and MAC
    // Data rate: 6Mbps OFDM
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue("OfdmRate6Mbps"));
    // OCB (Outside the Context of a BSS) mode
    Config::SetDefault("ns3::WaveMacHelper::Active", BooleanValue(true));
    // Use Control Channel (CCH)
    Config::SetDefault("ns3::WaveMacHelper::ChannelNumber", UintegerValue(172));
    Config::SetDefault("ns3::WaveMacHelper::CchEnabled", BooleanValue(true));

    YansWavePhyHelper wavePhyHelper = YansWavePhyHelper::Default();
    waveHelper.SetPhyHelper(wavePhyHelper);
    waveHelper.SetMacHelper(WaveMacHelper::Default());

    NetDeviceContainer waveDevices = waveHelper.Install(vehicles);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(waveDevices);

    // 6. Set up UDP application
    // Receiver (UdpServer) on vehicle 1
    uint16_t port = 9; // Discard port
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 0.1));

    // Sender (UdpClient) on vehicle 0
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));   // Max packets to send
    client.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send every 0.1 seconds
    client.SetAttribute("PacketSize", UintegerValue(1024));   // 1KB payload
    ApplicationContainer clientApps = client.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 0.5));

    // 7. Track packet transmissions and receptions
    Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(clientApps.Get(0));
    Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApps.Get(0));

    // Schedule printing statistics at the end of the simulation
    Simulator::Schedule(Seconds(simulationTime + 0.1), &PrintStats, udpClient, udpServer);

    // 8. Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}