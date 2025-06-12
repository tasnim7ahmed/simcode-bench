#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-phy-standard.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTimingParametersDemo");

static uint64_t g_bytesReceived = 0;

void ReceivePacket(Ptr<const Packet> packet, const Address &)
{
    g_bytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    double slotTime = 9.0;   // microseconds, default 802.11n
    double sifs = 10.0;      // microseconds, default 802.11n (SIFS)
    double pifs = 25.0;      // microseconds, default 802.11n (PIFS)

    // Simulation parameters
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1024;   // bytes
    uint32_t nPackets = 10000;
    double interval = 0.001;      // seconds

    CommandLine cmd;
    cmd.AddValue("slotTime", "Slot time in microseconds", slotTime);
    cmd.AddValue("sifs", "SIFS in microseconds", sifs);
    cmd.AddValue("pifs", "PIFS in microseconds", pifs);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ShortGuardEnabled", BooleanValue(true));
    phy.Set("ChannelWidth", UintegerValue(20));
    phy.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    // Adjust timing parameters in the DcfManager
    Config::SetDefault("ns3::DcfManager::SlotTime", TimeValue(MicroSeconds(slotTime)));
    Config::SetDefault("ns3::DcfManager::Sifs", TimeValue(MicroSeconds(sifs)));
    Config::SetDefault("ns3::DcfManager::Pifs", TimeValue(MicroSeconds(pifs)));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-timing-demo");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP server on STA (node 0)
    uint16_t port = 50000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1.0));

    // UDP client on AP (node 1)
    UdpClientHelper udpClient(staInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = udpClient.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    // Throughput calculation
    // Trace received packets on server
    wifiStaNode.Get(0)->GetApplication(0)->GetObject<UdpServer>()->TraceConnectWithoutContext(
        "Rx", MakeCallback(&ReceivePacket));

    Simulator::Stop(Seconds(simulationTime + 1.5));
    Simulator::Run();

    double throughput = (g_bytesReceived * 8.0) / (simulationTime * 1e6); // Mbps
    std::cout << "Simulation Duration: " << simulationTime << " s" << std::endl;
    std::cout << "SlotTime: " << slotTime << " us, SIFS: " << sifs << " us, PIFS: " << pifs << " us" << std::endl;
    std::cout << "Total Bytes Received: " << g_bytesReceived << std::endl;
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}