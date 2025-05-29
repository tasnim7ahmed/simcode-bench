#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMulticastExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application containers to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer senderNode;
    senderNode.Create(1);
    NodeContainer receiverNodes;
    receiverNodes.Create(3);
    NodeContainer allNodes;
    allNodes.Add(senderNode);
    allNodes.Add(receiverNodes);

    // Set up wireless channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure MAC layer
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    Ssid ssid = Ssid("multicast-network");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, receiverNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, senderNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
    interfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Configure UDP multicast client and server
    uint16_t port = 9; // Discard port (RFC 863)
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(receiverNodes);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(0), port); // Send to interface of sender
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(senderNode);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));


    // Enable tracing
    if (tracing) {
        phy.EnablePcap("wifi-multicast", apDevices.Get(0));
    }

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}