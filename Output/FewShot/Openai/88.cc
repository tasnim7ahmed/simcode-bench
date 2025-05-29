#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergySimulation");

static void
RemainingEnergyTrace(double oldValue, double remainingEnergy)
{
    NS_LOG_INFO("Remaining energy = " << remainingEnergy << " J");
    std::cout << Simulator::Now().GetSeconds() << "s: Remaining energy: " << remainingEnergy << " J" << std::endl;
}

static void
TotalEnergyConsumedTrace(double oldValue, double totalEnergy)
{
    NS_LOG_INFO("Total energy consumed = " << totalEnergy << " J");
    std::cout << Simulator::Now().GetSeconds() << "s: Energy consumed: " << totalEnergy << " J" << std::endl;
}

static void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &from)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Packet received. Size: " << packet->GetSize() << " bytes");
}

static void
TxTrace(std::string context, Ptr<const Packet> packet)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Packet sent. Size: " << packet->GetSize() << " bytes");
}

int main(int argc, char *argv[])
{
    uint32_t packetSize = 512;
    double distance = 20.0;
    double startTime = 1.0;
    double simulationTime = 10.0; // seconds
    double energyUpperLimit = 5.0; // Joules

    CommandLine cmd;
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("startTime", "Time when applications start (seconds)", startTime);
    cmd.AddValue("simulationTime", "Simulation stop time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    // Enable logs
    LogComponentEnable("WifiEnergySimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Wi-Fi PHY and channel configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-energy-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Energy source configuration
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(energyUpperLimit));
    EnergySourceContainer sources = basicSourceHelper.Install(wifiStaNodes);

    // Wi-Fi radio energy model
    WifiRadioEnergyModelHelper radioEnergyHelper;
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(staDevices, sources);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);

    // UDP traffic setup: node 0 -> node 1
    uint16_t port = 4000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(1));
    serverApps.Start(Seconds(startTime));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(startTime + 0.5));
    clientApps.Stop(Seconds(simulationTime));

    // Trace energy consumption for both nodes
    Ptr<EnergySource> src0 = sources.Get(0);
    Ptr<EnergySource> src1 = sources.Get(1);

    src0->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace));
    src0->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&TotalEnergyConsumedTrace));
    src1->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace));
    src1->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&TotalEnergyConsumedTrace));

    // Trace TX and RX
    Ptr<NetDevice> dev0 = staDevices.Get(0);
    Ptr<NetDevice> dev1 = staDevices.Get(1);
    dev0->TraceConnect("PhyTxEnd", "", MakeCallback(&TxTrace));
    dev1->TraceConnect("PhyRxEnd", "", MakeCallback(&RxTrace));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Report final energy status
    double finalEnergy0 = src0->GetRemainingEnergy();
    double finalEnergy1 = src1->GetRemainingEnergy();

    NS_LOG_UNCOND("Final remaining energy (Node 0): " << finalEnergy0 << " J");
    NS_LOG_UNCOND("Final remaining energy (Node 1): " << finalEnergy1 << " J");

    // Ensure energy consumed is within bounds
    if ((energyUpperLimit - finalEnergy0) > energyUpperLimit ||
        (energyUpperLimit - finalEnergy1) > energyUpperLimit)
    {
        NS_LOG_ERROR("Energy consumed exceeds the specified limit!");
    }
    else
    {
        NS_LOG_UNCOND("Energy consumption within specified limits.");
    }

    Simulator::Destroy();
    return 0;
}