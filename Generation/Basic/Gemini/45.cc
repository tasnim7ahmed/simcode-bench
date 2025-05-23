#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"

NS_LOG_COMPONENT_DEFINE("WifiInterferenceScenario");

int main(int argc, char *argv[])
{
    // Default configurable parameters
    double primaryRss = -80.0;     // Desired RSS at receiver from primary transmitter (dBm)
    double interferingRss = -70.0; // Desired RSS at receiver from interferer (dBm)
    double timeOffset = 0.0001;    // Time difference between primary and interfering transmission start (seconds)
    uint32_t primaryPacketSize = 1000; // Bytes
    uint32_t interfererPacketSize = 1000; // Bytes
    bool enableLog = false;        // Enable verbose Wi-Fi logging

    // Command line argument parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("primaryRss", "Desired RSS from primary transmitter at receiver (dBm)", primaryRss);
    cmd.AddValue("interferingRss", "Desired RSS from interferer at receiver (dBm)", interferingRss);
    cmd.AddValue("timeOffset", "Time offset between primary and interfering transmissions (seconds)", timeOffset);
    cmd.AddValue("primaryPacketSize", "Size of primary packet (bytes)", primaryPacketSize);
    cmd.AddValue("interfererPacketSize", "Size of interfering packet (bytes)", interfererPacketSize);
    cmd.AddValue("enableLog", "Enable verbose Wi-Fi logging (true/false)", enableLog);
    cmd.Parse(argc, argv);

    // Enable logging if requested
    if (enableLog)
    {
        LogComponentEnable("Wifi", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    // 1. Create Nodes: Transmitter, Receiver, Interferer
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> txNode = nodes.Get(0);
    Ptr<Node> rxNode = nodes.Get(1);
    Ptr<Node> interfererNode = nodes.Get(2);

    // 2. Set up Mobility: All nodes at the same location (0,0,0)
    // This simplifies RSS control using TxPower and ConstantPropagationLossModel.
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 3. Set up Channel and Propagation Loss Model
    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    // Using ConstantPropagationLossModel with 0 dB loss allows direct mapping of TxPower to RxPower (RSS)
    // at the receiver when all nodes are at the same location.
    channel->SetPropagationLossModel(CreateObject<ConstantPropagationLossModel>());
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    // 4. Set up PHY and MAC Layer (802.11b)
    YansWifiPhyHelper phy;
    phy.SetChannel(channel);
    // Set Pcap data link type for Wireshark compatibility
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    // Use AdhocWifiMac for simplicity and to avoid association overhead.
    // Adhoc MAC, combined with specific PHY settings for the interferer,
    // helps achieve "without carrier sense" behavior for interference.
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    // 5. Configure TxPower for individual nodes to achieve desired RSS
    // For txNode (primary transmitter)
    Ptr<WifiNetDevice> txDevice = DynamicCast<WifiNetDevice>(devices.Get(0));
    Ptr<YansWifiPhy> txPhy = DynamicCast<YansWifiPhy>(txDevice->GetPhy());
    txPhy->SetTxPower(primaryRss); // TxPower = desired RSS because loss is 0 dB

    // For interfererNode
    Ptr<WifiNetDevice> interfererDevice = DynamicCast<WifiNetDevice>(devices.Get(2));
    Ptr<YansWifiPhy> interfererPhy = DynamicCast<YansWifiPhy>(interfererDevice->GetPhy());
    interfererPhy->SetTxPower(interferingRss); // TxPower = desired RSS because loss is 0 dB

    // To ensure the interferer truly transmits "without carrier sense",
    // set its CCA (Clear Channel Assessment) and Energy Detection thresholds very low.
    // This effectively makes the interferer "deaf" to other signals, allowing it to transmit
    // even if another signal is present on the channel, thus acting as a true interferer.
    interfererPhy->SetEnergyDetectionThreshold(-120); // Very low to detect almost nothing
    interfererPhy->SetCcaMode1Threshold(-120);        // Very low to ignore other signals

    // 6. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 7. Setup Applications
    // Primary Transmission (TxNode -> RxNode)
    uint16_t primaryPort = 9;
    OnOffHelper primaryOnOff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), primaryPort));
    primaryOnOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]")); // Send one burst for 1ms
    primaryOnOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Only one burst
    primaryOnOff.SetAttribute("PacketSize", UintegerValue(primaryPacketSize));
    // Set a high data rate to ensure the packet is sent quickly within the OnTime
    primaryOnOff.SetAttribute("DataRate", StringValue("100Mbps"));
    ApplicationContainer primaryTxApps = primaryOnOff.Install(txNode);
    primaryTxApps.Start(Seconds(1.0));
    primaryTxApps.Stop(Seconds(1.002)); // Ensure it finishes transmission just after start

    // Receiver (RxNode) Packet Sink for primary transmission
    PacketSinkHelper primarySink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), primaryPort));
    ApplicationContainer primaryRxApps = primarySink.Install(rxNode);
    primaryRxApps.Start(Seconds(0.0));
    primaryRxApps.Stop(Seconds(10.0));

    // Interferer Transmission (InterfererNode -> RxNode, though destination doesn't strictly matter for interference)
    uint16_t interfererPort = 10;
    OnOffHelper interfererOnOff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), interfererPort)); // Send to RxNode
    interfererOnOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]")); // Send one burst for 1ms
    interfererOnOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Only one burst
    interfererOnOff.SetAttribute("PacketSize", UintegerValue(interfererPacketSize));
    interfererOnOff.SetAttribute("DataRate", StringValue("100Mbps"));
    ApplicationContainer interfererTxApps = interfererOnOff.Install(interfererNode);
    interfererTxApps.Start(Seconds(1.0 + timeOffset));
    interfererTxApps.Stop(Seconds(1.002 + timeOffset)); // Ensure it finishes transmission just after start

    // 8. Pcap Tracing
    phy.EnablePcapAll("wifi-interference");

    // 9. Run Simulation
    Simulator::Stop(Seconds(5.0)); // Simulate for 5 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}