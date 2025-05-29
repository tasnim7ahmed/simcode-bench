#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiStaApUdpExample");

uint32_t totalRx = 0;
uint32_t totalTx = 0;

void RxPacketCounter(Ptr<const Packet> packet, const Address &address) {
    totalRx += packet->GetSize();
}

void TxPacketCounter(Ptr<const Packet> packet) {
    totalTx += packet->GetSize();
}

int main(int argc, char *argv[]) {
    double simTime = 10.0;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 200;
    double interval = 0.05;

    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // WiFi Physical and MAC Layer configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211g");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility for nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application configuration
    uint16_t port = 4000;

    // UDP Server on STA2 (Node 1)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Client on STA1 (Node 0), targeting STA2
    UdpClientHelper udpClient(staInterfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Throughput / Packet loss tracing
    Ptr<UdpServer> udpServerPtr = DynamicCast<UdpServer>(serverApp.Get(0));
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback(&TxPacketCounter));

    // NetAnim
    AnimationInterface anim("wifi-sta-ap-udp.xml");
    anim.SetConstantPosition(wifiStaNodes.Get(0), 0.0, 0.0);    // STA1
    anim.SetConstantPosition(wifiApNode.Get(0), 5.0, 0.0);      // AP
    anim.SetConstantPosition(wifiStaNodes.Get(1), 10.0, 0.0);   // STA2

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Results
    uint32_t rx = udpServerPtr->GetReceived();
    uint32_t tx = numPackets;

    double throughput = (rx * packetSize * 8.0) / (simTime - 2) / 1e6; // Mbps
    std::cout << "Packets sent:     " << tx << std::endl;
    std::cout << "Packets received: " << rx << std::endl;
    std::cout << "Packet loss:      " << (tx - rx) << std::endl;
    std::cout << "Throughput:       " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}