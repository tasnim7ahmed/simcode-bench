#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/constant-position-mobility-model.h"

// Do not change the following line, it is needed for ns3.
using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2); // Node 0 (client), Node 1 (server)

    // 2. Set up mobility with static positions
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Ptr<ConstantPositionMobilityModel> node0Mobility = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    node0Mobility->SetPosition(Vector(0, 0, 0)); // Node 0 at (0,0)

    Ptr<ConstantPositionMobilityModel> node1Mobility = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    node1Mobility->SetPosition(Vector(5, 10, 0)); // Node 1 at (5,10)

    // 3. Install internet stack (TCP/IPv4)
    InternetStackHelper internet;
    internet.Install(nodes);

    // 4. Configure WiFi 802.11b with Aarf rate manager
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Using Adhoc for simplicity, no AP/STA distinction needed for this setup

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetStandard(WIFI_PHY_STANDARD_80211b); // Set 802.11b standard
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel"); // Simple propagation loss
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Set up TCP server (PacketSink) on Node 1
    uint16_t port = 9; // Specified port
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0)); // Run for the entire simulation duration

    // 7. Set up TCP client (OnOffApplication) on Node 0
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    OnOffHelper onoffHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port))); // Node 1's IP and port
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=20.0]")); // Keep client on for full duration
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffHelper.SetAttribute("PacketSize", UintegerValue(512)); // 512 bytes per packet
    
    // Calculate data rate: 100 packets * 512 bytes/packet / 0.1 seconds = 512000 bytes/second = 512 kbps
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("512kbps"))); 

    ApplicationContainer clientApps = onoffHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start client after 1 second to allow server to initialize
    clientApps.Stop(Seconds(20.0)); // Run until end of simulation

    // Optional: Enable PCAP tracing for debugging
    // wifiPhy.EnablePcap("wifi-tcp-aarf-node0", devices.Get(0));
    // wifiPhy.EnablePcap("wifi-tcp-aarf-node1", devices.Get(1));

    // 8. Run simulation
    Simulator::Stop(Seconds(20.0)); // Simulation duration
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}