#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

// Define namespace for convenience
using namespace ns3;

int main(int argc, char *argv[])
{
    // Create two nodes (client and server)
    NodeContainer nodes;
    nodes.Create(2);

    Ptr<Node> clientNode = nodes.Get(0);
    Ptr<Node> serverNode = nodes.Get(1);

    // Install mobility model (ConstantPosition for fixed nodes)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set positions for nodes (optional, but good for visualization if using NetAnim)
    Ptr<ConstantPositionMobilityModel> serverMobility = serverNode->GetObject<ConstantPositionMobilityModel>();
    serverMobility->SetPosition(Vector(0.0, 0.0, 0.0)); // Server at origin
    Ptr<ConstantPositionMobilityModel> clientMobility = clientNode->GetObject<ConstantPositionMobilityModel>();
    clientMobility->SetPosition(Vector(10.0, 0.0, 0.0)); // Client 10m away

    // Configure Wifi Channel
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel"); // Simple Friis propagation loss model

    // Configure Wifi PHY
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    // Set transmit power (optional)
    phy.Set("TxPowerStart", DoubleValue(20)); // Tx power in dBm
    phy.Set("TxPowerEnd", DoubleValue(20));

    // Configure Wifi MAC (Ad-hoc mode for direct communication between two nodes)
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Configure Wifi Helper for 1Gbps data rate
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac); // Use 802.11ac for high data rate capabilities
    // Set the data rate to 1Gbps using ConstantRateWifiManager
    // "OfdmRate1000Mbps" will be parsed by WifiModeFactory to set a PHY rate of 1 Gbps
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate1000Mbps"));

    // Install Wifi devices on nodes
    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    // Install Internet Stack on both nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP Addresses in the 192.168.1.0/24 range
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Get the server's IP address (assuming server is node 1)
    Ipv4Address serverIp = interfaces.GetAddress(1);

    // Configure and install Server (PacketSink) application
    uint16_t port = 9; // Arbitrary port for TCP communication
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(serverNode);
    serverApps.Start(Seconds(1.0)); // Server starts at 1s
    serverApps.Stop(Seconds(10.0)); // Server stops at 10s

    // Configure and install Client (OnOffApplication) application
    OnOffHelper onoff("ns3::TcpSocketFactory", Ipv4Address::GetZero()); // Placeholder address for remote
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(serverIp, port)));
    onoff.SetAttribute("PacketSize", UintegerValue(1024)); // Client sends 1KB packets
    // "sends packets at 0.1s intervals" means 10 packets per second.
    // For 1KB packets, this implies a data rate of 10 pkts/s * 1024 B/pkt * 8 bits/B = 81920 bps.
    onoff.SetAttribute("DataRate", DataRateValue("81920bps")); 
    // OnTime and OffTime define the burst behavior. For continuous sending during active time:
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=100.0]")); // Long enough to cover simulation
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));  // No off time

    ApplicationContainer clientApps = onoff.Install(clientNode);
    clientApps.Start(Seconds(2.0)); // Client starts sending at 2s
    clientApps.Stop(Seconds(10.0)); // Client stops sending at 10s

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}