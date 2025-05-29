/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiStaApUdpExample");

uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;
uint32_t g_lostPackets = 0;
uint64_t g_rxBytes = 0;

void
TxTrace(Ptr<const Packet> packet)
{
    g_txPackets++;
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
    g_rxPackets++;
    g_rxBytes += packet->GetSize();
}

void
DropTrace(Ptr<const Packet> packet)
{
    g_lostPackets++;
}

int
main(int argc, char *argv[])
{
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t packetSize = 1024;
    uint32_t numPackets = 2000;
    double interval = 0.01; // 10 ms
    double simulationTime = 20; // seconds

    // 1. Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // 2. Physical and MAC layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // 3. Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // Position STAs at (0,0,0) and (10,0,0), AP at (5,5,0)
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // STA 0
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));    // STA 1
    positionAlloc->Add(Vector(5.0, 5.0, 0.0));     // AP 0
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // 4. Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // 6. Applications
    // UDP server on STA1
    uint16_t udpPort = 9;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(wifiStaNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // UDP client on STA0, sending to STA1
    UdpClientHelper udpClient(staInterfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = udpClient.Install(wifiStaNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // 7. Packet tracing for metrics (install on STA1 if receiving)
    Ptr<NetDevice> sta1Device = staDevices.Get(1);
    sta1Device->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&DropTrace));

    Config::ConnectWithoutContext("/NodeList/0/DeviceList/0/Mac/MacTx", MakeCallback(&TxTrace));
    Config::ConnectWithoutContext("/NodeList/1/DeviceList/0/Mac/MacRx", MakeCallback(&RxTrace));

    // 8. NetAnim setup
    AnimationInterface anim("wifi-2sta-ap.xml"); // Output file for NetAnim
    anim.SetConstantPosition(wifiStaNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(wifiStaNodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(wifiApNode.Get(0), 5.0, 5.0);
    // Add labels
    anim.UpdateNodeDescription(wifiStaNodes.Get(0), "STA0");
    anim.UpdateNodeDescription(wifiStaNodes.Get(1), "STA1");
    anim.UpdateNodeDescription(wifiApNode.Get(0), "AP");

    // 9. Run simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // 10. Results
    double throughput = g_rxBytes * 8.0 / (simulationTime - 2.0) / 1e6; // Mbps
    NS_LOG_UNCOND("========= Simulation Results =========");
    NS_LOG_UNCOND("Packets sent:    " << g_txPackets);
    NS_LOG_UNCOND("Packets received:" << g_rxPackets);
    NS_LOG_UNCOND("Packets lost:    " << g_lostPackets);
    NS_LOG_UNCOND("Throughput:      " << throughput << " Mbps");
    NS_LOG_UNCOND("======================================");

    Simulator::Destroy();
    return 0;
}