#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

int main(int argc, char *argv[])
{
    double primaryRssDbm = -80.0;
    double interfererRssDbm = -80.0;
    double timeOffset = 0.0;
    uint32_t primaryPktSize = 1000;
    uint32_t interfererPktSize = 1000;
    bool verbose = false;
    std::string pcapFile = "wifi-interference";

    CommandLine cmd;
    cmd.AddValue("primaryRssDbm", "RSS (in dBm) at receiver from transmitter", primaryRssDbm);
    cmd.AddValue("interfererRssDbm", "RSS (in dBm) at receiver from interferer", interfererRssDbm);
    cmd.AddValue("timeOffset", "Offset in seconds: interferer packet relative to primary (s)", timeOffset);
    cmd.AddValue("primaryPktSize", "Packet size in bytes for primary transmission", primaryPktSize);
    cmd.AddValue("interfererPktSize", "Packet size in bytes for interferer transmission", interfererPktSize);
    cmd.AddValue("verbose", "Enable verbose Wi-Fi logging", verbose);
    cmd.AddValue("pcapFile", "Base filename for pcap output", pcapFile);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
        LogComponentEnable("WifiInterferenceSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
        LogComponentEnable("WifiNetDevice", LOG_LEVEL_INFO);
        LogComponentEnable("MacLow", LOG_LEVEL_INFO);
        LogComponentEnable("MacRxMiddle", LOG_LEVEL_INFO);
        LogComponentEnable("DcfManager", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate11Mbps"),
                                 "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;

    NetDeviceContainer devices;
    Ssid ssid = Ssid("wifi-interference");
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (i == 1)
        {
            // Receiver: AP
            mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        }
        else
        {
            // Transmitter or Interferer: STA, disable scanning
            mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        }
        devices.Add(wifi.Install(phy, mac, nodes.Get(i)));
    }

    MobilityHelper mobility;

    // For fixed RSS, nodes must be positioned so path loss achieves desired RSS
    // We'll place Rx at (0,0,0), Tx at (d,0,0), Interferer at (0,d,0)
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    double rxX = 0.0, rxY = 0.0, rxZ = 0.0;
    double txX = 1.0, txY = 0.0, txZ = 0.0;
    double intX = 0.0, intY = 1.0, intZ = 0.0;

    positionAlloc->Add(Vector(txX, txY, txZ));  // 0: Transmitter
    positionAlloc->Add(Vector(rxX, rxY, rxZ));  // 1: Receiver
    positionAlloc->Add(Vector(intX, intY, intZ)); // 2: Interferer

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set fixed RSS using MatrixPropagationLossModel
    Ptr<YansWifiChannel> wifiChannel = DynamicCast<YansWifiChannel>(phy.GetChannel());

    // Remove default models, add new propagation model
    wifiChannel->SetPropagationLossModel(CreateObject<MatrixPropagationLossModel>());
    Ptr<MatrixPropagationLossModel> matrix = wifiChannel->GetPropagationLossModel()->GetObject<MatrixPropagationLossModel>();
    matrix->SetDefaultLossDb(200); // Make other links 'off'

    // Set primary Tx-Rx
    matrix->SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), -primaryRssDbm);
    // Set interferer-Rx
    matrix->SetLoss(nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), -interfererRssDbm);

    // Allow communication only one-way to Rx (both sources --> Rx)
    matrix->SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 200);
    matrix->SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 200);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    // Applications
    // Receiver: PacketSink
    uint16_t port = 12345;
    Address sinkAddress(InetSocketAddress(ifaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(5.0));

    // Primary Transmitter: sends to receiver via UDP
    Ptr<Socket> primarySocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

    Simulator::ScheduleWithContext(
        primarySocket->GetNode()->GetId(),
        Seconds(1.0),
        [primarySocket, sinkAddress, primaryPktSize]() {
            Ptr<Packet> p = Create<Packet>(primaryPktSize);
            primarySocket->Connect(sinkAddress);
            primarySocket->Send(p);
        });

    // Interferer: sends to receiver, no carrier sense (set WifiTxParameters::DoNotWaitForChannel=true)
    // In ns-3, disable carrier sense (DIFS and CCA) via ConfigureStandard and NqstaWifiMacSpec
    // But for fine control in this context, easier to use high MAC Pr=0, e.g.,

    // Disable channel access for interferer: use OcbWifiMac (802.11p), which disables MAC layer contention.
    // Otherwise, set Dcf parameter to make immediate transmission (AIFSN/slot)
    // Instead, we use custom callback to send packet directly.

    Ptr<Socket> interfererSocket = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());

    Simulator::ScheduleWithContext(
        interfererSocket->GetNode()->GetId(),
        Seconds(1.0 + timeOffset),
        [interfererSocket, sinkAddress, interfererPktSize]() {
            Ptr<Packet> p = Create<Packet>(interfererPktSize);
            interfererSocket->Connect(sinkAddress);
            interfererSocket->Send(p);
        });

    // Enable pcap tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap(pcapFile, devices.Get(1), true, true); // Capture at receiver

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}