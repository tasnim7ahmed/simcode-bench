//  This example program can be used to experiment with spatial
//  reuse mechanisms of 802.11ax.
//
//  The geometry is as follows:
//
//                STA1          STA2
//                 |              |
//              d1 |              |d2
//                 |       d3     |
//                AP1 -----------AP2
//
//  STA1 and AP1 are in one BSS (with color set to 1), while STA2 and AP2 are in
//  another BSS (with color set to 2). The distances are configurable (d1 through d3).
//
//  STA1 is continuously transmitting data to AP1, while STA2 is continuously sending data to AP2.
//  Each STA has configurable traffic loads (inter packet interval and packet size).
//  It is also possible to configure TX power per node as well as their CCA-ED thresholds.
//  OBSS_PD spatial reuse feature can be enabled (default) or disabled, and the OBSS_PD
//  threshold can be set as well (default: -72 dBm).
//  A simple Friis path loss model is used and a constant PHY rate is considered.
//
//  In general, the program can be configured at run-time by passing command-line arguments.
//  The following command will display all of the available run-time help options:
//    ./ns3 run "wifi-spatial-reuse --help"
//
//  According to the Wi-Fi models of ns-3.36 release, throughput with
//  OBSS PD enabled (--enableObssPd=True) was roughly 6.5 Mbps, and with
//  it disabled (--enableObssPd=False) was roughly 3.5 Mbps.
//
//  This difference between those results is because OBSS_PD spatial
//  reuse enables to ignore transmissions from another BSS when the
//  received power is below the configured threshold, and therefore
//  either defer during ongoing transmission or transmit at the same
//  time.
//
//  Note that, by default, this script configures a network using a
//  channel width of 20 MHz. Changing this value alone (through
//  the --channelWidth argument) without properly adjusting other
//  parameters will void the effect of spatial reuse: since energy is
//  measured over the 20 MHz primary channel regardless of the channel
//  width, doubling the transmission bandwidth creates a 3 dB drop in
//  the measured energy level (i.e., a halving of the measured
//  energy). Because of this, when using the channelWidth argument
//  users should also adjust the CCA-ED Thresholds (via --ccaEdTrSta1,
//  --ccaEdTrSta2, --ccaEdTrAp1, and --ccaEdTrAp2), the Minimum RSSI
//  for preamble detection (via --minimumRssi), and the OBSS PD
//  Threshold (via --obssPdThreshold) appropriately. For instance,
//  this can be accomplished for a channel width of 40 MHz by lowering
//  all these values by 3 dB compared to their defaults.
//
//  In addition, consider that adapting the adjustments shown above
//  for an 80 MHz channel width (using a 6 dB threshold adjustment instead
//  of 3 dB) will not produce any changes when enableObssPd is enabled
//  or disabled. The cause for this is the insufficient amount of
//  traffic that is generated by default in the example: increasing
//  the channel width shortens the frame durations, and lowers the
//  collision probability. Collisions between BSSs are a necessary
//  condition to see the improvements brought by spatial reuse, and
//  thus increasing the amount of generated traffic by setting the
//  interval argument to a lower value is necessary to see the
//  benefits of spatial reuse in this scenario. This can, for
//  instance, be accomplished by setting --interval=100us.
//
//  Spatial reuse reset events are traced in two text files:
//  - wifi-spatial-reuse-resets-bss-1.txt (for STA 1)
//  - wifi-spatial-reuse-resets-bss-2.txt (for STA 2)

#include "ns3/abort.h"
#include "ns3/ap-wifi-mac.h"
#include "ns3/application-container.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/he-configuration.h"
#include "ns3/he-phy.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/obss-pd-algorithm.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-server.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/wifi-net-device.h"

using namespace ns3;

std::vector<uint32_t> bytesReceived(4);
std::ofstream g_resetFile1;
std::ofstream g_resetFile2;

uint32_t
ContextToNodeId(std::string context)
{
    std::string sub = context.substr(10);
    uint32_t pos = sub.find("/Device");
    return std::stoi(sub.substr(0, pos));
}

void
SocketRx(std::string context, Ptr<const Packet> p, const Address& addr)
{
    uint32_t nodeId = ContextToNodeId(context);
    bytesReceived[nodeId] += p->GetSize();
}

void
ResetTrace(std::string context,
           uint8_t bssColor,
           double rssiDbm,
           bool powerRestricted,
           double txPowerMaxDbmSiso,
           double txPowerMaxDbmMimo)
{
    if (context == "1")
    {
        g_resetFile1 << Simulator::Now().GetSeconds() << " bssColor: " << +bssColor
                     << " rssiDbm: " << rssiDbm << " powerRestricted: " << powerRestricted
                     << " txPowerMaxDbmSiso: " << txPowerMaxDbmSiso
                     << " txPowerMaxDbmMimo: " << txPowerMaxDbmMimo << std::endl;
    }
    else if (context == "2")
    {
        g_resetFile2 << Simulator::Now().GetSeconds() << " bssColor: " << +bssColor
                     << " rssiDbm: " << rssiDbm << " powerRestricted: " << powerRestricted
                     << " txPowerMaxDbmSiso: " << txPowerMaxDbmSiso
                     << " txPowerMaxDbmMimo: " << txPowerMaxDbmMimo << std::endl;
    }
    else
    {
        NS_FATAL_ERROR("Unknown context " << context);
    }
}

int
main(int argc, char* argv[])
{
    Time duration{"10s"};
    meter_u d1{30.0};
    meter_u d2{30.0};
    meter_u d3{150.0};
    dBm_u powSta1{10.0};
    dBm_u powSta2{10.0};
    dBm_u powAp1{21.0};
    dBm_u powAp2{21.0};
    dBm_u ccaEdTrSta1{-62};
    dBm_u ccaEdTrSta2{-62};
    dBm_u ccaEdTrAp1{-62};
    dBm_u ccaEdTrAp2{-62};
    dBm_u minimumRssi{-82};
    int channelWidth{20};       // MHz
    uint32_t payloadSize{1500}; // bytes
    uint32_t mcs{0};            // MCS value
    Time interval{"1ms"};
    bool enableObssPd{true};
    dBm_u obssPdThreshold{-72.0};

    CommandLine cmd(__FILE__);
    cmd.AddValue("duration", "Duration of simulation", duration);
    cmd.AddValue("interval", "Inter packet interval", interval);
    cmd.AddValue("enableObssPd", "Enable/disable OBSS_PD", enableObssPd);
    cmd.AddValue("d1", "Distance between STA1 and AP1 (m)", d1);
    cmd.AddValue("d2", "Distance between STA2 and AP2 (m)", d2);
    cmd.AddValue("d3", "Distance between AP1 and AP2 (m)", d3);
    cmd.AddValue("powSta1", "Power of STA1 (dBm)", powSta1);
    cmd.AddValue("powSta2", "Power of STA2 (dBm)", powSta2);
    cmd.AddValue("powAp1", "Power of AP1 (dBm)", powAp1);
    cmd.AddValue("powAp2", "Power of AP2 (dBm)", powAp2);
    cmd.AddValue("ccaEdTrSta1", "CCA-ED Threshold of STA1 (dBm)", ccaEdTrSta1);
    cmd.AddValue("ccaEdTrSta2", "CCA-ED Threshold of STA2 (dBm)", ccaEdTrSta2);
    cmd.AddValue("ccaEdTrAp1", "CCA-ED Threshold of AP1 (dBm)", ccaEdTrAp1);
    cmd.AddValue("ccaEdTrAp2", "CCA-ED Threshold of AP2 (dBm)", ccaEdTrAp2);
    cmd.AddValue("minimumRssi",
                 "Minimum RSSI for the ThresholdPreambleDetectionModel",
                 minimumRssi);
    cmd.AddValue("channelWidth", "Bandwidth of the channel in MHz [20, 40, or 80]", channelWidth);
    cmd.AddValue("obssPdThreshold", "Threshold for the OBSS PD Algorithm", obssPdThreshold);
    cmd.AddValue("mcs", "The constant MCS value to transmit HE PPDUs", mcs);
    cmd.Parse(argc, argv);

    g_resetFile1.open("wifi-spatial-reuse-resets-bss-1.txt", std::ofstream::out);
    g_resetFile2.open("wifi-spatial-reuse-resets-bss-2.txt", std::ofstream::out);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    SpectrumWifiPhyHelper spectrumPhy;
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
    spectrumChannel->AddPropagationLossModel(lossModel);
    Ptr<ConstantSpeedPropagationDelayModel> delayModel =
        CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->SetPropagationDelayModel(delayModel);

    spectrumPhy.SetChannel(spectrumChannel);
    spectrumPhy.SetErrorRateModel("ns3::YansErrorRateModel");
    switch (channelWidth)
    {
    case 20:
        spectrumPhy.Set("ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));
        break;
    case 40:
        spectrumPhy.Set("ChannelSettings", StringValue("{62, 40, BAND_5GHZ, 0}"));
        break;
    case 80:
        spectrumPhy.Set("ChannelSettings", StringValue("{171, 80, BAND_5GHZ, 0}"));
        break;
    default:
        NS_ABORT_MSG("Unrecognized channel width: " << channelWidth);
        break;
    }
    spectrumPhy.SetPreambleDetectionModel("ns3::ThresholdPreambleDetectionModel",
                                          "MinimumRssi",
                                          DoubleValue(minimumRssi));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    if (enableObssPd)
    {
        wifi.SetObssPdAlgorithm("ns3::ConstantObssPdAlgorithm",
                                "ObssPdLevel",
                                DoubleValue(obssPdThreshold));
    }

    WifiMacHelper mac;
    std::ostringstream oss;
    oss << "HeMcs" << mcs;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(oss.str()),
                                 "ControlMode",
                                 StringValue(oss.str()));

    spectrumPhy.Set("TxPowerStart", DoubleValue(powSta1));
    spectrumPhy.Set("TxPowerEnd", DoubleValue(powSta1));
    spectrumPhy.Set("CcaEdThreshold", DoubleValue(ccaEdTrSta1));
    spectrumPhy.Set("RxSensitivity", DoubleValue(-92.0));

    Ssid ssidA = Ssid("A");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidA));
    NetDeviceContainer staDeviceA = wifi.Install(spectrumPhy, mac, wifiStaNodes.Get(0));

    spectrumPhy.Set("TxPowerStart", DoubleValue(powAp1));
    spectrumPhy.Set("TxPowerEnd", DoubleValue(powAp1));
    spectrumPhy.Set("CcaEdThreshold", DoubleValue(ccaEdTrAp1));
    spectrumPhy.Set("RxSensitivity", DoubleValue(-92.0));

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidA));
    NetDeviceContainer apDeviceA = wifi.Install(spectrumPhy, mac, wifiApNodes.Get(0));

    Ptr<WifiNetDevice> apDevice = apDeviceA.Get(0)->GetObject<WifiNetDevice>();
    Ptr<ApWifiMac> apWifiMac = apDevice->GetMac()->GetObject<ApWifiMac>();
    if (enableObssPd)
    {
        apDevice->GetHeConfiguration()->SetAttribute("BssColor", UintegerValue(1));
    }

    spectrumPhy.Set("TxPowerStart", DoubleValue(powSta2));
    spectrumPhy.Set("TxPowerEnd", DoubleValue(powSta2));
    spectrumPhy.Set("CcaEdThreshold", DoubleValue(ccaEdTrSta2));
    spectrumPhy.Set("RxSensitivity", DoubleValue(-92.0));

    Ssid ssidB = Ssid("B");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidB));
    NetDeviceContainer staDeviceB = wifi.Install(spectrumPhy, mac, wifiStaNodes.Get(1));

    spectrumPhy.Set("TxPowerStart", DoubleValue(powAp2));
    spectrumPhy.Set("TxPowerEnd", DoubleValue(powAp2));
    spectrumPhy.Set("CcaEdThreshold", DoubleValue(ccaEdTrAp2));
    spectrumPhy.Set("RxSensitivity", DoubleValue(-92.0));

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidB));
    NetDeviceContainer apDeviceB = wifi.Install(spectrumPhy, mac, wifiApNodes.Get(1));

    Ptr<WifiNetDevice> ap2Device = apDeviceB.Get(0)->GetObject<WifiNetDevice>();
    apWifiMac = ap2Device->GetMac()->GetObject<ApWifiMac>();
    if (enableObssPd)
    {
        ap2Device->GetHeConfiguration()->SetAttribute("BssColor", UintegerValue(2));
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP1
    positionAlloc->Add(Vector(d3, 0.0, 0.0));  // AP2
    positionAlloc->Add(Vector(0.0, d1, 0.0));  // STA1
    positionAlloc->Add(Vector(d3, d2, 0.0));   // STA2
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(wifiApNodes);
    mobility.Install(wifiStaNodes);

    PacketSocketHelper packetSocket;
    packetSocket.Install(wifiApNodes);
    packetSocket.Install(wifiStaNodes);
    ApplicationContainer apps;

    // BSS 1
    {
        PacketSocketAddress socketAddr;
        socketAddr.SetSingleDevice(staDeviceA.Get(0)->GetIfIndex());
        socketAddr.SetPhysicalAddress(apDeviceA.Get(0)->GetAddress());
        socketAddr.SetProtocol(1);
        Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
        client->SetRemote(socketAddr);
        wifiStaNodes.Get(0)->AddApplication(client);
        client->SetAttribute("PacketSize", UintegerValue(payloadSize));
        client->SetAttribute("MaxPackets", UintegerValue(0));
        client->SetAttribute("Interval", TimeValue(interval));
        Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
        server->SetLocal(socketAddr);
        wifiApNodes.Get(0)->AddApplication(server);
    }

    // BSS 2
    {
        PacketSocketAddress socketAddr;
        socketAddr.SetSingleDevice(staDeviceB.Get(0)->GetIfIndex());
        socketAddr.SetPhysicalAddress(apDeviceB.Get(0)->GetAddress());
        socketAddr.SetProtocol(1);
        Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
        client->SetRemote(socketAddr);
        wifiStaNodes.Get(1)->AddApplication(client);
        client->SetAttribute("PacketSize", UintegerValue(payloadSize));
        client->SetAttribute("MaxPackets", UintegerValue(0));
        client->SetAttribute("Interval", TimeValue(interval));
        Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
        server->SetLocal(socketAddr);
        wifiApNodes.Get(1)->AddApplication(server);
    }

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSocketServer/Rx",
                    MakeCallback(&SocketRx));

    // Obtain pointers to the ObssPdAlgorithm objects and hook trace sinks
    // to the Reset trace source on each STA.  Note that this trace connection
    // cannot be done through the Config path system, so pointers are used.
    auto deviceA = staDeviceA.Get(0)->GetObject<WifiNetDevice>();
    auto hePhyA = DynamicCast<HePhy>(deviceA->GetPhy()->GetPhyEntity(WIFI_MOD_CLASS_HE));
    // Pass in the context string "1" to allow the trace to distinguish objects
    hePhyA->GetObssPdAlgorithm()->TraceConnect("Reset", "1", MakeCallback(&ResetTrace));
    auto deviceB = staDeviceB.Get(0)->GetObject<WifiNetDevice>();
    auto hePhyB = DynamicCast<HePhy>(deviceB->GetPhy()->GetPhyEntity(WIFI_MOD_CLASS_HE));
    // Pass in the context string "2" to allow the trace to distinguish objects
    hePhyB->GetObssPdAlgorithm()->TraceConnect("Reset", "2", MakeCallback(&ResetTrace));

    Simulator::Stop(duration);
    Simulator::Run();

    Simulator::Destroy();

    for (uint32_t i = 0; i < 2; i++)
    {
        const auto throughput = bytesReceived[2 + i] * 8.0 / duration.GetMicroSeconds();
        std::cout << "Throughput for BSS " << i + 1 << ": " << throughput << " Mbit/s" << std::endl;
    }

    g_resetFile1.close();
    g_resetFile2.close();

    return 0;
}
