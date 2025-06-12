#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-client-server-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessNetworkSimulation");

void
PrintPhyState(uint32_t nodeId, Time start, Time duration)
{
    NS_LOG_UNCOND("Node " << nodeId << " PHY state changed to: " << Simulator::Now().GetSeconds() << "s (Duration: " << duration.GetSeconds() << "s)");
}

void
TraceMacRx(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
    *stream->GetStream() << "MAC RX: " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << "s" << std::endl;
}

void
TracePhyRxOk(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, double snr)
{
    *stream->GetStream() << "PHY RX OK: " << packet->GetSize() << " bytes, SNR: " << snr << " at " << Simulator::Now().GetSeconds() << "s" << std::endl;
}

void
TracePhyRxFailure(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, double snr)
{
    *stream->GetStream() << "PHY RX ERROR: " << packet->GetSize() << " bytes, SNR: " << snr << " at " << Simulator::Now().GetSeconds() << "s" << std::endl;
}

void
MoveAp(Ptr<Node> apNode, Ptr<MobilityModel> mobility)
{
    Vector position = mobility->GetPosition();
    position.x += 5.0;
    mobility->SetPosition(Vector(position.x, position.y, position.z));
    Simulator::Schedule(Seconds(1.0), &MoveAp, apNode, mobility);
}

int
main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("WirelessNetworkSimulation", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Setup Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate54Mbps"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    Ssid ssid = Ssid("ns3-wifi");
    WifiMacHelper mac;

    // Setup AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Setup STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Assign constant velocity for movement
    for (NodeContainer::Iterator it = wifiStaNodes.Begin(); it != wifiStaNodes.End(); ++it)
    {
        Ptr<MobilityModel> mobility = (*it)->GetObject<MobilityModel>();
        mobility->SetPosition(Vector(0.0, 0.0, 0.0));
        mobility->SetVelocity(Vector(0.0, 0.0, 0.0));
    }

    // Move AP helper function
    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    Simulator::Schedule(Seconds(1.0), &MoveAp, wifiApNode.Get(0), apMobility);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Install packet socket capabilities
    PacketSocketHelper packetSocket;
    packetSocket.Install(wifiApNode);
    packetSocket.Install(wifiStaNodes);

    // Setup communication between stations and access point
    PacketSocketAddress socketAddress(Ipv4Address::GetAny(), 80, 0);

    // Client-server application
    PacketSocketClientHelper clientHelper(socketAddress);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(1000)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    PacketSocketServerHelper serverHelper(socketAddress);

    ApplicationContainer serverApp = serverHelper.Install(wifiApNode);
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(44.0));

    ApplicationContainer clientApps[2];
    for (int i = 0; i < 2; ++i)
    {
        clientApps[i] = clientHelper.Install(wifiStaNodes.Get(i));
        clientApps[i].Start(Seconds(0.5));
        clientApps[i].Stop(Seconds(44.0));
    }

    // Tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> phyRxOk = asciiTraceHelper.CreateFileStream("phy_rx_ok.tr");
    Ptr<OutputStreamWrapper> phyRxFail = asciiTraceHelper.CreateFileStream("phy_rx_fail.tr");
    Ptr<OutputStreamWrapper> macRx = asciiTraceHelper.CreateFileStream("mac_rx.tr");

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk", MakeBoundCallback(&TracePhyRxOk, phyRxOk));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxError", MakeBoundCallback(&TracePhyRxFailure, phyRxFail));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeBoundCallback(&TraceMacRx, macRx));

    // Connect PHY state tracing
    for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i)
    {
        Ptr<Node> node = NodeList::GetNode(i);
        for (uint32_t j = 0; j < node->GetNDevices(); ++j)
        {
            if (DynamicCast<WifiNetDevice>(node->GetDevice(j)))
            {
                DynamicCast<WifiNetDevice>(node->GetDevice(j))->GetPhy()->TraceConnectWithoutContext("State", 
                    MakeBoundCallback(&PrintPhyState, node->GetId()));
            }
        }
    }

    Simulator::Stop(Seconds(44.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}