#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <iomanip>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridWiredWirelessTcpExample");

uint64_t totalBytesReceived = 0;
Time flowStartTime;
Time flowEndTime;

void
RxCallback(Ptr<const Packet> packet, const Address &)
{
    totalBytesReceived += packet->GetSize();
    flowEndTime = Simulator::Now();
}

int main(int argc, char *argv[])
{
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // Simulation parameters
    uint32_t numWiredNodes = 4;
    uint32_t numWirelessNodes = 3;
    double simulationTime = 10.0; // seconds

    // Create nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(numWiredNodes);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numWirelessNodes);

    Ptr<Node> wifiApNode = wiredNodes.Get(1); // Wired node 1 is the Wi-Fi gateway

    // ========== Point-to-point chain among wired nodes ==========
    std::vector<NetDeviceContainer> p2pDevices(numWiredNodes - 1);
    std::vector<NodeContainer> p2pNodeContainers(numWiredNodes - 1);
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    for (uint32_t i = 0; i < numWiredNodes - 1; ++i)
    {
        p2pNodeContainers[i] = NodeContainer(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        p2pDevices[i] = p2p.Install(p2pNodeContainers[i]);
    }

    // ========== Wi-Fi network ==========
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("hybrid-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility for Wi-Fi nodes (fixed position, but realistic)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(25.0, 0.0, 0.0));  // AP position
    for (uint32_t i = 0; i < numWirelessNodes; ++i) {
        posAlloc->Add(Vector(25.0 + 5*i, 10.0, 0.0));
    }
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // ========== Internet Stack ==========
    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    // ========== Addressing ==========
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> p2pInterfaces(numWiredNodes - 1);

    // Assign addresses for p2p links
    for (uint32_t i = 0; i < numWiredNodes - 1; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        p2pInterfaces[i] = address.Assign(p2pDevices[i]);
    }

    // Assign addresses for Wi-Fi
    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // ========== Routing ==========
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ========== TCP Applications ==========
    // We will send TCP traffic from each STA to the last wired node
    uint16_t port = 50000;

    Address sinkAddress(InetSocketAddress(p2pInterfaces.back().GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);

    ApplicationContainer sinkApps = sinkHelper.Install(wiredNodes.Get(numWiredNodes - 1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime + 1));

    // Each STA will run a BulkSendApplication to the sink
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numWirelessNodes; ++i)
    {
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited
        ApplicationContainer app = bulkSend.Install(wifiStaNodes.Get(i));
        app.Start(Seconds(1.0 + i * 0.2)); // Staggered start
        app.Stop(Seconds(simulationTime));
        clientApps.Add(app);
    }

    // ========== Tracing ==========
    // Enable pcap tracing for p2p
    for (uint32_t i = 0; i < numWiredNodes - 1; ++i)
    {
        p2p.EnablePcapAll("hybrid-p2p-" + std::to_string(i));
    }
    // Enable pcap tracing for Wi-Fi
    phy.EnablePcapAll("hybrid-wifi", false);

    // Throughput calculation
    // Only consider bytes received at sink after flow start time
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));
    flowStartTime = Seconds(1.0);

    // ========== Run Simulation ==========
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // ========== Throughput Calculation ==========
    double throughputMbps = (totalBytesReceived * 8.0) / (simulationTime * 1e6); // Mb/s

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Simulation time: " << simulationTime << " s\n";
    std::cout << "Total bytes received at sink: " << totalBytesReceived << " bytes\n";
    std::cout << "Average TCP throughput: " << throughputMbps << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}