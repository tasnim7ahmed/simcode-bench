#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/config.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenStationMpduAggregation");

static void
ThroughputMonitor(Ptr<PacketSink> sink, double prevBytes, Time prevTime, double simulationTime)
{
    double cur = Simulator::Now().GetSeconds();
    if (cur > simulationTime)
        return;

    double bytes = sink->GetTotalRx() - prevBytes;
    double throughput = (bytes * 8.0) / 1e6 / (cur - prevTime.GetSeconds());  // Mbps

    std::cout << "At " << cur << "s: Throughput = " << throughput << " Mbps" << std::endl;
    Simulator::Schedule(Seconds(1.0), &ThroughputMonitor, sink, sink->GetTotalRx(), Simulator::Now(), simulationTime);
}

int main(int argc, char *argv[])
{
    // Parameters with default values
    bool enableRtsCts = false;
    uint32_t ampduNumMpdus = 32;
    uint32_t payloadSize = 1024; // bytes
    double simulationTime = 10.0; // seconds
    double throughputLower = 0.0;
    double throughputUpper = 1000.0;

    CommandLine cmd;
    cmd.AddValue("rtsCts", "Enable RTS/CTS (true/false)", enableRtsCts);
    cmd.AddValue("ampduNumMpdus", "Number of MPDUs in A-MPDU (aggregation)", ampduNumMpdus);
    cmd.AddValue("payloadSize", "Size of application payload in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("throughputLower", "Expected lower bound of throughput (Mbps)", throughputLower);
    cmd.AddValue("throughputUpper", "Expected upper bound of throughput (Mbps)", throughputUpper);
    cmd.Parse(argc, argv);

    // Create nodes: 2 STAs + 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up physical (channel + PHY)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> chPtr = channel.Create();

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(chPtr);
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Set Wi-Fi standard to 802.11n
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    // Configure MAC
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    // Configure aggregation (MPDU aggregation for 802.11n)
    // Limit the maximum number of aggregated MPDUs in an A-MPDU
    Config::SetDefault("ns3::HtWifiMac::MaxAmsduSize", UintegerValue(7935));
    Config::SetDefault("ns3::HtWifiMac::MaximumSupportedAmpduLength", UintegerValue(ampduNumMpdus * 4095)); // 4095 = max MPDU size for 802.11n
    // The next is an overall cap for the number of MPDUs per A-MPDU
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmpduSize", UintegerValue(ampduNumMpdus * 4095));

    // Enable/disable RTS/CTS
    if (enableRtsCts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(0));
        std::cout << "RTS/CTS enabled." << std::endl;
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2347));
        std::cout << "RTS/CTS disabled." << std::endl;
    }

    // STA MACs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility: hidden node scenario
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // Place AP at (0,0,0), STA1 at (-20,0,0), STA2 at (20,0,0)
    positionAlloc->Add(Vector(-20.0, 0.0, 0.0)); // STA1
    positionAlloc->Add(Vector(20.0, 0.0, 0.0));  // STA2
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Limit the PHY RX range so STAs can't hear each other, but both can reach AP
    phy.Set("RxGain", DoubleValue(0.0));
    // Use Friis model
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel",
                              "Frequency", DoubleValue(2.4e9),
                              "SystemLoss", DoubleValue(1.0),
                              "MinLoss", DoubleValue(0.0));

    // Lower transmit power if necessary (ensure 40m separation is out of range)
    phy.Set("TxPowerStart", DoubleValue(14.0)); // dBm
    phy.Set("TxPowerEnd", DoubleValue(14.0));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP traffic: Each STA sends UDP to AP
    uint16_t port1 = 4000;
    uint16_t port2 = 4001;

    // Packet sink on the AP (to receive from both)
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApps = sinkHelper.Install(wifiApNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime + 1));

    // UdpClient for STA1
    UdpClientHelper client1(apInterface.GetAddress(0), port1);
    client1.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client1.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client1.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp1 = client1.Install(wifiStaNodes.Get(0));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(simulationTime + 1));

    // UdpClient for STA2
    UdpClientHelper client2(apInterface.GetAddress(0), port1);
    client2.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client2.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client2.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp2 = client2.Install(wifiStaNodes.Get(1));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(simulationTime + 1));

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("hidden-amdu.tr"));
    phy.EnablePcapAll("hidden-amdu", false);

    // Throughput monitor (per second)
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    Simulator::Schedule(Seconds(1.0), &ThroughputMonitor, sink, 0, Seconds(0.0), simulationTime);

    // Flow monitor (optional, to help bound throughput)
    // Uncomment if needed for further analysis:
    /*
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    */

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    double totalRx = sink->GetTotalRx();
    double throughput = (totalRx * 8.0) / simulationTime / 1e6; // Mbps

    std::cout << "Simulation finished!\n";
    std::cout << "Total received: " << totalRx << " bytes\n";
    std::cout << "Average throughput: " << throughput << " Mbps\n";

    if (throughput < throughputLower || throughput > throughputUpper)
    {
        std::cout << "WARNING: Throughput out of expected bounds [" << throughputLower << ", " << throughputUpper << "] Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}