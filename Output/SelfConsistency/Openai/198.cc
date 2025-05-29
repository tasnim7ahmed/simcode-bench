/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Vehicle-to-Vehicle (V2V) communication simulation using ns-3 Wave (802.11p)
 * Two vehicles communicate using UDP over Wave.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveUdpExample");

// Global variables to collect statistics
uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;

void
TxCallback(Ptr<const Packet> packet)
{
    g_txPackets++;
}

void
RxCallback(Ptr<const Packet> packet, const Address &address)
{
    g_rxPackets++;
}

int
main(int argc, char *argv[])
{
    // LogLevel
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numVehicles = 2;
    double distance = 50.0;  // meters, vehicles separation
    double simulationTime = 5.0; // seconds
    uint16_t wavePort = 4001;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // 1. Create two nodes
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // 2. Set up mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));
    posAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(vehicles);

    // 3. Setup Wave (802.11p)
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, vehicles);

    // 4. Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Install UDP server on vehicle 2 (node 1)
    uint16_t udpPort = wavePort;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(vehicles.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Trace packet receptions at server
    serverApps.Get(0)->GetObject<UdpServer>()->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    // 7. Install UDP client on vehicle 1 (node 0)
    uint32_t packetSize = 200; // bytes
    uint32_t maxPackets = 10;
    Time interPacketInterval = Seconds(0.5);
    UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = udpClient.Install(vehicles.Get(0));
    clientApps.Start(Seconds(1.0)); // Start after server is up
    clientApps.Stop(Seconds(simulationTime));

    // Trace packet transmissions at client
    clientApps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&TxCallback));

    // 8. Animation (optional)
    // Uncomment to generate NetAnim trace file
    // AnimationInterface anim("v2v-wave.xml");

    // 9. Run simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // 10. Gather statistics
    std::cout << "========== V2V Wave/UDP Statistics ==========" << std::endl;
    std::cout << "Packets sent   : " << g_txPackets << std::endl;
    std::cout << "Packets received: " << g_rxPackets << std::endl;
    Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApps.Get(0));
    if (server)
    {
        std::cout << "Server received packets (UdpServer count): "
                  << server->GetReceived() << std::endl;
    }
    std::cout << "=============================================" << std::endl;

    Simulator::Destroy();
    return 0;
}