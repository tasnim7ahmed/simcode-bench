#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer senderNode;
    senderNode.Create(1);
    NodeContainer receiverNodes;
    receiverNodes.Create(3);
    NodeContainer allNodes = NodeContainer(senderNode, receiverNodes);

    // Set up wireless channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("multicast-network");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, receiverNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(1.0)));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, senderNode);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(staDevices);
    interfaces = address.Assign(apDevices);

    // Set mobility model
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

    // Configure multicast address
    Ipv4Address multicastAddress("225.1.2.3");

    // Set up UDP Server (Receiver) on each receiver node
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(receiverNodes);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Join multicast group
    for (uint32_t i = 0; i < receiverNodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = receiverNodes.Get(i)->GetObject<Ipv4>();
        ipv4->SetMulticastForwarding(true);
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
            Ipv4InterfaceAddress iface = ipv4->GetAddress(j, 0);
            if (iface.GetLocal() == interfaces.GetAddress(i).Get()) {
                ipv4->AddMulticastRoute(multicastAddress, j);
                ipv4->AddJoinedGroup(j, multicastAddress);
            }
        }
    }

    // Set up UDP Client (Sender)
    UdpClientHelper client(multicastAddress, 9);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(senderNode);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}