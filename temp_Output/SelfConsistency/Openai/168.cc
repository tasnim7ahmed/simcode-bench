#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInfraUdpThroughputExample");

uint64_t totalBytesReceived = 0;

void ReceivePacket (Ptr<const Packet> packet, const Address &)
{
    totalBytesReceived += packet->GetSize ();
}

int main (int argc, char *argv[])
{
    uint32_t nStations = 4;
    double simulationTime = 10.0; // seconds
    uint32_t payloadSize = 1024; // bytes
    std::string dataRate = "54Mbps";
    std::string phyDelay = "2ms";

    CommandLine cmd (__FILE__);
    cmd.AddValue ("nStations", "Number of station (client) nodes", nStations);
    cmd.Parse (argc, argv);

    // Create nodes: nStations STAs, 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    channel.SetPropagationDelay ("ns3:ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3:LogDistancePropagationLossModel");
    channel.SetPropagationDelay ("ns3:ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> chan = channel.Create ();
    chan->SetAttribute ("Delay", TimeValue (MilliSeconds (2)));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (chan);
    phy.Set ("TxPowerStart", DoubleValue (16));
    phy.Set ("TxPowerEnd", DoubleValue (16));
    phy.Set ("RxGain", DoubleValue (0));
    phy.Set ("TxGain", DoubleValue (0));

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi-ssid");

    // Configure MAC for STA
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    // Configure MAC for AP
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiApNode);

    // Enable 54Mbps rate for all stations/AP
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue ("OfdmRate54Mbps"),
                                 "ControlMode", StringValue ("OfdmRate24Mbps"));

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes);
    mobility.Install (wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (wifiStaNodes);
    stack.Install (wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign (staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign (apDevice);

    // UDP server on first STA
    uint16_t port = 4000;
    UdpServerHelper udpServer (port);
    ApplicationContainer serverApp = udpServer.Install (wifiStaNodes.Get (0));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simulationTime));

    // UDP client on second STA
    UdpClientHelper udpClient (staInterfaces.GetAddress (0), port);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (-1)));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    udpClient.SetAttribute ("StartTime", TimeValue (Seconds(2.0)));
    udpClient.SetAttribute ("StopTime", TimeValue (Seconds(simulationTime)));

    ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (1));

    // For throughput calculation: connect the server socket receive callback
    Ptr<Node> serverNode = wifiStaNodes.Get (0);
    Ptr<Ipv4> ipv4 = serverNode->GetObject<Ipv4> ();
    Ipv4Address serverAddress = ipv4->GetAddress (1, 0).GetLocal ();

    // For server's UdpServer, attach a trace callback to Rx
    Ptr<UdpServer> udpServerApp = DynamicCast<UdpServer> (serverApp.Get (0));
    udpServerApp->TraceConnectWithoutContext ("Rx", MakeCallback (&ReceivePacket));

    // Enable Tracing
    phy.EnablePcapAll ("wifi-infra");
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll (ascii.CreateFileStream ("wifi-infra.tr"));

    // Run simulation
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();

    // Throughput calculation
    double throughput = (totalBytesReceived * 8) / (simulationTime - 2.0) / 1e6; // [Mbit/s]
    std::cout << "Simulation Time: " << simulationTime << " s" << std::endl;
    std::cout << "Total bytes received by server: " << totalBytesReceived << " bytes" << std::endl;
    std::cout << "Measured throughput: " << throughput << " Mbit/s" << std::endl;

    Simulator::Destroy ();
    return 0;
}