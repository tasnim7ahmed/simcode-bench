#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/pcap-file-wrapper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

uint64_t totalBytesReceived = 0;

void ReceivePacket(Ptr<const Packet> packet, const Address &address)
{
    totalBytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    uint32_t nStations = 4;
    double simulationTime = 10.0; // seconds
    std::string dataRate = "54Mbps";
    std::string delay = "2ms";
    uint16_t udpPort = 50000;

    CommandLine cmd;
    cmd.AddValue("nStations", "Number of Wi-Fi stations", nStations);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    Ptr<YansWifiChannel> chan = channel.Create();
    chan->SetAttribute("Delay", TimeValue(Time(delay)));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(chan);
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("ErpOfdmRate54Mbps"),
                                "ControlMode", StringValue("ErpOfdmRate24Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nStations; ++i)
    {
        positionAlloc->Add(Vector(2.0 * i, 0.0, 0.0));
    }
    positionAlloc->Add(Vector(5.0, 5.0, 0.0)); // AP position
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP server on station 0
    uint32_t serverNodeIdx = 0;
    uint32_t clientNodeIdx = 1;

    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(wifiStaNodes.Get(serverNodeIdx));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // UDP client on station 1 sending to server (station 0)
    UdpClientHelper udpClient(staInterfaces.GetAddress(serverNodeIdx), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(32000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10))); // 100 packets/sec
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = udpClient.Install(wifiStaNodes.Get(clientNodeIdx));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable PCAP and ASCII tracing
    phy.EnablePcapAll("wifi-infra");
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-infra.tr"));

    // Trace the bytes received at the server
    Ptr<Node> serverNode = wifiStaNodes.Get(serverNodeIdx);
    Ptr<Ipv4> ipv4 = serverNode->GetObject<Ipv4>();
    Ipv4Address serverAddr = ipv4->GetAddress(1, 0).GetLocal();
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(serverNode->GetId()) + "/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> p, const Address &a) {
        totalBytesReceived += p->GetSize();
    }));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    double throughput = (totalBytesReceived * 8.0) / (simulationTime - 2.0) / 1e6; // Mbps, subtract app start time

    std::cout << "Total Bytes Received by Server: " << totalBytesReceived << std::endl;
    std::cout << "Average Throughput at Server: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}