#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"

using namespace ns3;

int main()
{
    // Simulation parameters
    double simulationTime = 10.0; // seconds
    uint32_t numNodes = 4;
    uint32_t payloadSize = 512;
    uint32_t packetCount = 100;
    double clientStartTime = 1.0;
    double clientStopTime = simulationTime - 0.1;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Mobility (Constant Position)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(numNodes),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Configure Wifi (Ad-hoc)
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    WifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("TxPowerStart", DoubleValue(7.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(7.0));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    NetDeviceContainer devices = wifi.Install(phy, wifiMac, nodes);

    // Configure Internet Stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup Applications
    // Node 3 (index 3) as UDP Echo Server
    uint16_t port = 9; // Echo port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Node 0 (index 0) as UDP Echo Client
    // Get the IP address of Node 3
    Ipv4Address serverAddress = interfaces.GetAddress(3);
    UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(clientStartTime));
    clientApps.Stop(Seconds(clientStopTime));

    // Enable PCAP Tracing
    phy.EnablePcap("adhoc-aodv", devices);

    // Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}