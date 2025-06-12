#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTimingParametersDemo");

int main(int argc, char *argv[])
{
    double simulationTime = 10.0; // seconds

    // Wi-Fi timing parameters (microseconds)
    uint16_t slotTime = 9;
    uint16_t sifs = 10;
    uint16_t pifs = 25;

    CommandLine cmd;
    cmd.AddValue("slotTime", "Slot time in microseconds", slotTime);
    cmd.AddValue("sifs", "SIFS in microseconds", sifs);
    cmd.AddValue("pifs", "PIFS in microseconds", pifs);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Create nodes: AP and STA
    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    // Wifi PHY and channel setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set 802.11n standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    // MAC helpers
    Ssid ssid = Ssid("ns3-ssid");
    WifiMacHelper mac;

    // Wi-Fi timing parameters (can also set DcfManager attributes)
    // Attributes are in nanoseconds; convert from microseconds
    uint64_t slotTimeNs = slotTime * 1000ull;
    uint64_t sifsNs = sifs * 1000ull;
    uint64_t pifsNs = pifs * 1000ull;

    // Set timing parameters for Sta wifi MAC
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    phy.Set("Slot", TimeValue(NanoSeconds(slotTimeNs)));
    phy.Set("Sifs", TimeValue(NanoSeconds(sifsNs)));
    Config::SetDefault("ns3::DcfManager::SlotTime", TimeValue(NanoSeconds(slotTimeNs)));
    Config::SetDefault("ns3::DcfManager::Sifs", TimeValue(NanoSeconds(sifsNs)));
    Config::SetDefault("ns3::DcfManager::Pifs", TimeValue(NanoSeconds(pifsNs)));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // AP device
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility: simple static positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // STA
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // AP
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // UDP server on STA
    uint16_t port = 50000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1));

    // UDP client on AP
    uint32_t payloadSize = 1472; // bytes
    uint32_t interPacketInterval = 1000; // us
    UdpClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(interPacketInterval)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime + 1));

    // Enable pcap tracing (for debug, optional)
    WifiHelper::EnablePcapAll("wifi-timing-demo", false);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 2));
    Simulator::Run();

    // Calculate throughput
    uint64_t totalBytesReceived = 0;
    Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp.Get(0));
    if (udpServer)
    {
        totalBytesReceived = udpServer->GetReceived();
        double throughput = (totalBytesReceived * payloadSize * 8.0) / (simulationTime * 1000000.0); // Mbps
        std::cout << "Simulation time: " << simulationTime << " s\n";
        std::cout << "Slot time: " << slotTime << " us, SIFS: " << sifs << " us, PIFS: " << pifs << " us\n";
        std::cout << "Total packets received: " << totalBytesReceived << std::endl;
        std::cout << "UDP Downlink Throughput: " << throughput << " Mbps\n";
    }
    else
    {
        std::cout << "Error: Couldn't obtain UdpServer pointer\n";
    }

    Simulator::Destroy();
    return 0;
}