#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Allow configuration from command line, though not strictly needed for this problem
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // 1. Wi-Fi Configuration
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n); // Using 802.11n as a modern standard

    YansWifiPhyHelper wifiPhy;
    // Set up the Wi-Fi Channel
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // 2. Create Nodes: AP (sender) and STA (receiver)
    NodeContainer nodes;
    nodes.Create(2); // Create 2 nodes
    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // 3. Configure MAC layer for AP and STA
    Ssid ssid = Ssid("ns3-wifi-network");
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();

    // Configure AP MAC
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(MilliSeconds(100)));
    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install(wifiPhy, wifiMac, apNode);

    // Configure STA MAC
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false)); // STA doesn't need to probe if SSID is known
    NetDeviceContainer staDevice;
    staDevice = wifiHelper.Install(wifiPhy, wifiMac, staNode);

    // 4. Install Internet Stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);

    // Get assigned IP addresses for sender (AP) and receiver (STA)
    Ipv4Address senderIp = apInterfaces.GetAddress(0);   // AP is sender
    Ipv4Address receiverIp = staInterfaces.GetAddress(0); // STA is receiver

    // 6. Mobility (Static Mobility)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set specific positions for the nodes to ensure they are distinct
    Ptr<ConstantPositionMobilityModel> apMobility = apNode->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0)); // AP at origin

    Ptr<ConstantPositionMobilityModel> staMobility = staNode->GetObject<ConstantPositionMobilityModel>();
    staMobility->SetPosition(Vector(10.0, 0.0, 0.0)); // STA 10 meters away from AP

    // 7. Applications

    // Receiver (STA) - UDP Server
    uint16_t serverPort = 9; // UDP server on port 9
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApps = server.Install(staNode);
    serverApps.Start(Seconds(0.0)); // Start server at simulation beginning
    serverApps.Stop(Seconds(25.0)); // Run for longer than client to ensure all packets are received

    // Sender (AP) - UDP Client
    uint32_t packetSize = 1024; // Bytes
    uint32_t numPackets = 1000;
    Time interPacketInterval = MilliSeconds(20);

    UdpClientHelper client(receiverIp, serverPort); // Destination IP and port
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(apNode); // Install on AP node
    clientApps.Start(Seconds(1.0)); // Start client a bit after server to ensure server is ready

    // Calculate the time required for all packets to be sent
    double clientSendDuration = numPackets * interPacketInterval.GetSeconds();
    double clientStopTime = 1.0 + clientSendDuration + 1.0; // Start time + send duration + buffer
    clientApps.Stop(Seconds(clientStopTime));

    // Optional: Enable PCAP tracing for debugging/analysis
    wifiPhy.EnablePcap("wifi-sender", apDevice.Get(0));
    wifiPhy.EnablePcap("wifi-receiver", staDevice.Get(0));

    // Set overall simulation stop time
    Simulator::Stop(Seconds(clientStopTime + 3.0)); // Ensure simulation runs long enough for all packets

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}