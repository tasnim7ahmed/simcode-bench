#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTimingParametersExample");

double g_totalBytes = 0.0;

void
RxPacket(Ptr<const Packet> packet, const Address &address)
{
    g_totalBytes += packet->GetSize();
}

int main(int argc, char *argv[])
{
    double simTime = 10.0;
    uint16_t port = 50000;
    double slotTime = 9.0;     // microseconds, 802.11n 2.4GHz default
    double sifs = 10.0;        // microseconds
    double pifs = 19.0;        // microseconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("slotTime", "Slot time in microseconds", slotTime);
    cmd.AddValue("sifs", "SIFS in microseconds", sifs);
    cmd.AddValue("pifs", "PIFS in microseconds", pifs);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    // Physical Channel & PHY Helpers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;

    // Set slot time, SIFS, PIFS via object factory
    Config::SetDefault("ns3::WifiPhy::SlotTime", TimeValue(MicroSeconds(slotTime)));
    Config::SetDefault("ns3::WifiPhy::Sifs", TimeValue(MicroSeconds(sifs)));
    Config::SetDefault("ns3::WifiPhy::Pifs", TimeValue(MicroSeconds(pifs)));

    Ssid ssid = Ssid("wifi-timing-demo");

    // STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility Model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // STA
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));   // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP Server on STA
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Client on AP
    uint32_t maxPacketCount = 10000;
    Time interPacketInterval = Seconds(0.001);  // 1ms
    uint32_t packetSize = 1472;
    UdpClientHelper udpClient(staInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = udpClient.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Throughput calculation
    Config::Connect("/NodeList/0/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&RxPacket));

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    double throughput = (g_totalBytes * 8) / (simTime - 2.0) / 1e6; // Mbps; account for app start after 2s
    std::cout << "Simulation finished!" << std::endl;
    std::cout << "SlotTime: " << slotTime << "us, SIFS: " << sifs << "us, PIFS: " << pifs << "us" << std::endl;
    std::cout << "Total received bytes: " << g_totalBytes << std::endl;
    std::cout << "UDP Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}