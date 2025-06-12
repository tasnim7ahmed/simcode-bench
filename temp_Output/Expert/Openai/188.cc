#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpFlowNetAnim");

void
CalculateThroughputAndPacketLoss(Ptr<UdpServer> server, uint32_t prevRx, double prevTime, double &lastRx, double &lastTime)
{
    uint32_t curRx = server->GetReceived();
    double curTime = Simulator::Now().GetSeconds();
    double throughput = ((curRx - prevRx) * 1024 * 8) / (curTime - prevTime) / 1e6; // Mbps
    uint32_t totalRx = server->GetReceived();
    uint32_t totalTx = DynamicCast<UdpServer>(server)->GetLost() + totalRx;
    uint32_t packetLoss = DynamicCast<UdpServer>(server)->GetLost();
    std::cout << "At " << curTime << " s: Throughput: " << throughput << " Mbps, "
              << "Packets Received: " << totalRx << ", "
              << "Packet Loss: " << packetLoss << std::endl;
    lastRx = curRx;
    lastTime = curTime;
    Simulator::Schedule(Seconds(1.0), &CalculateThroughputAndPacketLoss, server, curRx, curTime, lastRx, lastTime);
}

int
main (int argc, char *argv[])
{
    uint32_t nSta = 2;
    double simulationTime = 10.0;
    std::string phyMode = "HtMcs7";
    CommandLine cmd;
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode",StringValue(phyMode),"ControlMode",StringValue(phyMode));

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi");

    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (5.0),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes);
    mobility.Install (wifiApNode);

    InternetStackHelper stack;
    stack.Install (wifiStaNodes);
    stack.Install (wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign (staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign (apDevice);

    uint16_t port = 5000;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simulationTime));

    uint32_t packetSize = 1024;
    uint32_t numPackets = 32000;
    double interPacketInterval = 0.005; // 200 pkt/s

    UdpClientHelper client (staInterfaces.GetAddress(1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    client.SetAttribute ("Interval", TimeValue (Seconds(interPacketInterval)));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApp = client.Install (wifiStaNodes.Get(0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (simulationTime));

    AnimationInterface anim("wifi-udp-flow.xml");
    anim.SetConstantPosition(wifiStaNodes.Get(0), 0, 0);
    anim.SetConstantPosition(wifiApNode.Get(0), 5, 0);
    anim.SetConstantPosition(wifiStaNodes.Get(1), 10, 0);

    Simulator::Schedule(Seconds(2.1), &CalculateThroughputAndPacketLoss,
                        DynamicCast<UdpServer>(serverApp.Get(0)),
                        0, 2.1, std::ref(*(new double)), std::ref(*(new double)));
    Simulator::Stop(Seconds(simulationTime+1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}