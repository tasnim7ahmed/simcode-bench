#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularV2VSimulation");

// Global statistics
uint32_t g_rxPackets = 0;
uint32_t g_txPackets = 0;
uint32_t g_lostPackets = 0;
double   g_delaySum = 0.0;

std::map<uint64_t, Time> g_sentTimes;

void
TxTrace(Ptr<const Packet> packet)
{
    g_txPackets++;
    g_sentTimes[packet->GetUid()] = Simulator::Now();
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
    g_rxPackets++;
    auto it = g_sentTimes.find(packet->GetUid());
    if (it != g_sentTimes.end())
    {
        Time delay = Simulator::Now() - it->second;
        g_delaySum += delay.GetSeconds();
    }
}

void
LostPacketTrace(std::string context, Ptr<const Packet> packet)
{
    g_lostPackets++;
}

int
main(int argc, char *argv[])
{
    uint32_t nVehicles = 4;
    double simulationTime = 20.0; // seconds
    double nodeDistance = 50.0; // meters between vehicles

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer vehicles;
    vehicles.Create(nVehicles);

    // Mobility: Constant velocity along a straight road
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        positionAlloc->Add(Vector(i * nodeDistance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> cvmm = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        cvmm->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s along X axis
    }

    // WiFi ad-hoc setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // Internet stack (IPv4)
    InternetStackHelper internet;
    internet.Install(vehicles);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP applications: Each vehicle sends to all others
    uint16_t port = 4000;
    double interval = 1.0; // one packet per second
    uint32_t packetSize = 512; // bytes

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        // Install a UDP server on each vehicle
        UdpServerHelper server(port + i);
        ApplicationContainer serverApp = server.Install(vehicles.Get(i));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime));
        serverApps.Add(serverApp);
    }

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        for (uint32_t j = 0; j < nVehicles; ++j)
        {
            if (i == j) continue; // Don't send to self
            UdpClientHelper client(interfaces.GetAddress(j), port + j);
            client.SetAttribute("MaxPackets", UintegerValue(10000));
            client.SetAttribute("Interval", TimeValue(Seconds(interval)));
            client.SetAttribute("PacketSize", UintegerValue(packetSize));
            ApplicationContainer clientApp = client.Install(vehicles.Get(i));
            clientApp.Start(Seconds(1.0) + Seconds(0.1 * i)); // avoid all apps starting at exact same time
            clientApp.Stop(Seconds(simulationTime));
            clientApps.Add(clientApp);
        }
    }

    // Packet loss and delay tracing
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&TxTrace));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&LostPacketTrace));

    // NetAnim setup
    AnimationInterface anim("vehicular_v2v_netanim.xml");
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        std::ostringstream oss;
        oss << "Vehicle " << i;
        anim.UpdateNodeDescription(vehicles.Get(i), oss.str());
        anim.UpdateNodeColor(vehicles.Get(i), 255 - i * 60, 60 + i * 60, 100); // Some color variation
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Collect stats
    uint32_t totalRx = 0;
    uint32_t totalTx = g_txPackets;
    uint32_t totalLost = g_lostPackets;
    double avgDelay = (g_rxPackets > 0) ? (g_delaySum / g_rxPackets) : 0.0;

    std::cout << "Simulation Results:" << std::endl;
    std::cout << "  Packets sent:     " << totalTx << std::endl;
    std::cout << "  Packets received: " << g_rxPackets << std::endl;
    std::cout << "  Packets lost:     " << totalLost << std::endl;
    std::cout << "  Average delay:    " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}