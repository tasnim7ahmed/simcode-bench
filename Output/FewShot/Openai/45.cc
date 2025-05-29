#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSim");

int main(int argc, char *argv[])
{
    // Default parameters
    double primaryRssDbm = -80.0;
    double interferingRssDbm = -80.0;
    double timeOffset = 0.0; // seconds; interferer sends at this offset after transmitter
    uint32_t primaryPktSize = 500;       // Bytes
    uint32_t interferingPktSize = 500;   // Bytes
    bool verbose = false;
    std::string pcapFile = "wifi-interference-sim";

    CommandLine cmd;
    cmd.AddValue("primaryRssDbm", "RSS (dBm) at receiver from transmitter", primaryRssDbm);
    cmd.AddValue("interferingRssDbm", "RSS (dBm) at receiver from interferer", interferingRssDbm);
    cmd.AddValue("timeOffset", "Time offset (seconds) for interfering packet", timeOffset);
    cmd.AddValue("primaryPktSize", "Primary packet size in bytes", primaryPktSize);
    cmd.AddValue("interferingPktSize", "Interferer packet size in bytes", interferingPktSize);
    cmd.AddValue("verbose", "Enable verbose wifi logging", verbose);
    cmd.AddValue("pcapFile", "Base filename for PCAP tracing", pcapFile);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("YansWifiPhy", LOG_LEVEL_ALL);
        LogComponentEnable("YansWifiChannel", LOG_LEVEL_ALL);
        LogComponentEnable("MacLow", LOG_LEVEL_ALL);
        LogComponentEnable("WifiNetDevice", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(3); // 0: transmitter, 1: receiver, 2: interferer

    // Mobility and positions (arbitrary since we control RSS directly)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Channel and propagation with specified RSS
    Ptr<ConstantPositionPropagationLossModel> propLoss1 = CreateObject<ConstantPositionPropagationLossModel>();
    Ptr<ConstantPositionPropagationLossModel> propLoss2 = CreateObject<ConstantPositionPropagationLossModel>();

    propLoss1->SetLoss(0, 1, -primaryRssDbm);      // Channel gain so RSS = primaryRssDbm
    propLoss1->SetLoss(2, 1, -interferingRssDbm);  // Will not be used for tx 2->1
    propLoss2->SetLoss(0, 1, -primaryRssDbm);
    propLoss2->SetLoss(2, 1, -interferingRssDbm);

    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    Ptr<YansWifiPropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
    channel->SetPropagationDelayModel(delay);
    channel->SetPropagationLossModel(propLoss1);

    // Wi-Fi PHY/MAC configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate1Mbps"));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel);
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-interf-test");

    NetDeviceContainer devices;
    // Node 0: transmitter - STA
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, nodes.Get(0)));
    // Node 1: receiver - AP
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, nodes.Get(1)));
    // Node 2: interferer - STA, but with no carrier sense
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, nodes.Get(2)));

    // Internet stack and addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Packet sink (UDP) on receiver (node 1)
    uint16_t port = 50000;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // -- Primary transmitter: simple UDP packet --
    Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    InetSocketAddress dstAddr(interfaces.GetAddress(1), port);

    // -- Interfering transmitter (raw 802.11, no carrier sense) --
    Ptr<WifiNetDevice> interferenceDev = DynamicCast<WifiNetDevice>(devices.Get(2));
    Ptr<YansWifiPhy> interferencePhy = DynamicCast<YansWifiPhy>(interferenceDev->GetPhy());
    // Disable CCA for interference
    interferencePhy->SetAttribute("CcaMode1Threshold", DoubleValue(-300.0)); // Disabled

    Ptr<Socket> intSocket = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());

    // Schedule packet sends
    Simulator::Schedule(Seconds(1.0), [srcSocket, dstAddr, primaryPktSize] {
        Ptr<Packet> p = Create<Packet>(primaryPktSize);
        srcSocket->SendTo(p, 0, dstAddr);
    });

    Simulator::Schedule(Seconds(1.0 + timeOffset), [intSocket, interfaces, port, interferingPktSize] {
        InetSocketAddress recvAddr(interfaces.GetAddress(1), port);
        Ptr<Packet> p = Create<Packet>(interferingPktSize);
        intSocket->SendTo(p, 0, recvAddr);
    });

    // Enable pcap tracing
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap(pcapFile + "-sta0", devices.Get(0));
    phy.EnablePcap(pcapFile + "-ap", devices.Get(1));
    phy.EnablePcap(pcapFile + "-sta2", devices.Get(2));

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}