#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-server.h"
#include "ns3/waveform-generator-helper.h"
#include "ns3/waveform-generator.h"
#include "ns3/wifi-net-device.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

#include <iomanip>

// This is a simple example of an IEEE 802.11n Wi-Fi network with a
// non-Wi-Fi interferer.  It is an adaptation of the wifi-spectrum-per-example
//
// Unless the --waveformPower argument is passed, it will operate similarly to
// wifi-spectrum-per-example.  Adding --waveformPower=value for values
// greater than 0.0001 will result in frame losses beyond those that
// result from the normal SNR based on distance path loss.
//
// If YansWifiPhy is selected as the wifiType, --waveformPower will have
// no effect.
//
// Network topology:
//
//  Wi-Fi 192.168.1.0
//
//   STA                  AP
//    * <-- distance -->  *
//    |                   |
//    n1                  n2
//
// Users may vary the following command-line arguments in addition to the
// attributes, global values, and default values typically available:
//
//    --simulationTime:  Simulation time [10s]
//    --udp:             UDP if set to 1, TCP otherwise [true]
//    --distance:        meters separation between nodes [50]
//    --index:           restrict index to single value between 0 and 31 [256]
//    --wifiType:        select ns3::SpectrumWifiPhy or ns3::YansWifiPhy [ns3::SpectrumWifiPhy]
//    --errorModelType:  select ns3::NistErrorRateModel or ns3::YansErrorRateModel
//    [ns3::NistErrorRateModel]
//    --enablePcap:      enable pcap output [false]
//    --waveformPower:   Waveform power (linear W) [0]
//
// By default, the program will step through 32 index values, corresponding
// to the following MCS, channel width, and guard interval combinations:
//   index 0-7:    MCS 0-7, long guard interval, 20 MHz channel
//   index 8-15:   MCS 0-7, short guard interval, 20 MHz channel
//   index 16-23:  MCS 0-7, long guard interval, 40 MHz channel
//   index 24-31:  MCS 0-7, short guard interval, 40 MHz channel
// and send UDP for 10 seconds using each MCS, using the SpectrumWifiPhy and the
// NistErrorRateModel, at a distance of 50 meters.  The program outputs
// results such as:
//
// wifiType: ns3::SpectrumWifiPhy distance: 50m; time: 10; TxPower: 16 dBm (40 mW)
// index   MCS  Rate (Mb/s) Tput (Mb/s) Received Signal (dBm)Noi+Inf(dBm) SNR (dB)
//     0     0      6.50        5.77    7414      -64.69      -93.97       29.27
//     1     1     13.00       11.58   14892      -64.69      -93.97       29.27
//     2     2     19.50       17.39   22358      -64.69      -93.97       29.27
//     3     3     26.00       23.23   29875      -64.69      -93.97       29.27
//   ...
//

using namespace ns3;

// Global variables for use in callbacks.
double g_signalDbmAvg; //!< Average signal power [dBm]
double g_noiseDbmAvg;  //!< Average noise power [dBm]
uint32_t g_samples;    //!< Number of samples

/**
 * Monitor sniffer Rx trace
 *
 * \param packet The sensed packet.
 * \param channelFreqMhz The channel frequency [MHz].
 * \param txVector The Tx vector.
 * \param aMpdu The aMPDU.
 * \param signalNoise The signal and noise dBm.
 * \param staId The STA ID.
 */
void
MonitorSniffRx(Ptr<const Packet> packet,
               uint16_t channelFreqMhz,
               WifiTxVector txVector,
               MpduInfo aMpdu,
               SignalNoiseDbm signalNoise,
               uint16_t staId)

{
    g_samples++;
    g_signalDbmAvg += ((signalNoise.signal - g_signalDbmAvg) / g_samples);
    g_noiseDbmAvg += ((signalNoise.noise - g_noiseDbmAvg) / g_samples);
}

NS_LOG_COMPONENT_DEFINE("WifiSpectrumPerInterference");

Ptr<SpectrumModel> SpectrumModelWifi5180MHz; //!< Spectrum model at 5180 MHz.
Ptr<SpectrumModel> SpectrumModelWifi5190MHz; //!< Spectrum model at 5190 MHz.

/** Initializer for a static spectrum model centered around 5180 MHz */
class static_SpectrumModelWifi5180MHz_initializer
{
  public:
    static_SpectrumModelWifi5180MHz_initializer()
    {
        BandInfo bandInfo;
        bandInfo.fc = 5180e6;
        bandInfo.fl = 5180e6 - 10e6;
        bandInfo.fh = 5180e6 + 10e6;

        Bands bands;
        bands.push_back(bandInfo);

        SpectrumModelWifi5180MHz = Create<SpectrumModel>(bands);
    }
};

/// Static instance to initizlize the spectrum model around 5180 MHz.
static_SpectrumModelWifi5180MHz_initializer static_SpectrumModelWifi5180MHz_initializer_instance;

/** Initializer for a static spectrum model centered around 5190 MHz */
class static_SpectrumModelWifi5190MHz_initializer
{
  public:
    static_SpectrumModelWifi5190MHz_initializer()
    {
        BandInfo bandInfo;
        bandInfo.fc = 5190e6;
        bandInfo.fl = 5190e6 - 10e6;
        bandInfo.fh = 5190e6 + 10e6;

        Bands bands;
        bands.push_back(bandInfo);

        SpectrumModelWifi5190MHz = Create<SpectrumModel>(bands);
    }
};

/// Static instance to initizlize the spectrum model around 5190 MHz.
static_SpectrumModelWifi5190MHz_initializer static_SpectrumModelWifi5190MHz_initializer_instance;

int
main(int argc, char* argv[])
{
    bool udp{true};
    meter_u distance{50};
    Time simulationTime{"10s"};
    uint16_t index{256};
    std::string wifiType{"ns3::SpectrumWifiPhy"};
    std::string errorModelType{"ns3::NistErrorRateModel"};
    bool enablePcap{false};
    const uint32_t tcpPacketSize{1448};
    Watt_u waveformPower{0};

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("udp", "UDP if set to 1, TCP otherwise", udp);
    cmd.AddValue("distance", "meters separation between nodes", distance);
    cmd.AddValue("index", "restrict index to single value between 0 and 31", index);
    cmd.AddValue("wifiType", "select ns3::SpectrumWifiPhy or ns3::YansWifiPhy", wifiType);
    cmd.AddValue("errorModelType",
                 "select ns3::NistErrorRateModel or ns3::YansErrorRateModel",
                 errorModelType);
    cmd.AddValue("enablePcap", "enable pcap output", enablePcap);
    cmd.AddValue("waveformPower", "Waveform power (linear W)", waveformPower);
    cmd.Parse(argc, argv);

    uint16_t startIndex = 0;
    uint16_t stopIndex = 31;
    if (index < 32)
    {
        startIndex = index;
        stopIndex = index;
    }

    std::cout << "wifiType: " << wifiType << " distance: " << distance
              << "m; time: " << simulationTime << "; TxPower: 16 dBm (40 mW)" << std::endl;
    std::cout << std::setw(5) << "index" << std::setw(6) << "MCS" << std::setw(13) << "Rate (Mb/s)"
              << std::setw(12) << "Tput (Mb/s)" << std::setw(10) << "Received " << std::setw(12)
              << "Signal (dBm)" << std::setw(12) << "Noi+Inf(dBm)" << std::setw(9) << "SNR (dB)"
              << std::endl;
    for (uint16_t i = startIndex; i <= stopIndex; i++)
    {
        uint32_t payloadSize;
        if (udp)
        {
            payloadSize = 972; // 1000 bytes IPv4
        }
        else
        {
            payloadSize = 1448; // 1500 bytes IPv6
            Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
        }

        NodeContainer wifiStaNode;
        wifiStaNode.Create(1);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);
        NodeContainer interferingNode;
        interferingNode.Create(1);

        YansWifiPhyHelper phy;
        SpectrumWifiPhyHelper spectrumPhy;
        Ptr<MultiModelSpectrumChannel> spectrumChannel;
        uint16_t frequency = (i <= 15 ? 5180 : 5190);
        if (wifiType == "ns3::YansWifiPhy")
        {
            YansWifiChannelHelper channel;
            channel.AddPropagationLoss("ns3::FriisPropagationLossModel",
                                       "Frequency",
                                       DoubleValue(frequency * 1e6));
            channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
            phy.SetChannel(channel.Create());
            phy.Set("ChannelSettings",
                    StringValue(std::string("{") + (frequency == 5180 ? "36" : "38") +
                                ", 0, BAND_5GHZ, 0}"));
        }
        else if (wifiType == "ns3::SpectrumWifiPhy")
        {
            spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
            Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
            lossModel->SetFrequency(frequency * 1e6);
            spectrumChannel->AddPropagationLossModel(lossModel);

            Ptr<ConstantSpeedPropagationDelayModel> delayModel =
                CreateObject<ConstantSpeedPropagationDelayModel>();
            spectrumChannel->SetPropagationDelayModel(delayModel);

            spectrumPhy.SetChannel(spectrumChannel);
            spectrumPhy.SetErrorRateModel(errorModelType);
            // channel 36 at 20 MHz, 38 at 40 MHz
            spectrumPhy.Set("ChannelSettings",
                            StringValue(std::string("{") + (frequency == 5180 ? "36" : "38") +
                                        ", 0, BAND_5GHZ, 0}"));
        }
        else
        {
            NS_FATAL_ERROR("Unsupported WiFi type " << wifiType);
        }

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper mac;

        Ssid ssid = Ssid("ns380211n");

        double datarate = 0;
        StringValue DataRate;
        if (i == 0)
        {
            DataRate = StringValue("HtMcs0");
            datarate = 6.5;
        }
        else if (i == 1)
        {
            DataRate = StringValue("HtMcs1");
            datarate = 13;
        }
        else if (i == 2)
        {
            DataRate = StringValue("HtMcs2");
            datarate = 19.5;
        }
        else if (i == 3)
        {
            DataRate = StringValue("HtMcs3");
            datarate = 26;
        }
        else if (i == 4)
        {
            DataRate = StringValue("HtMcs4");
            datarate = 39;
        }
        else if (i == 5)
        {
            DataRate = StringValue("HtMcs5");
            datarate = 52;
        }
        else if (i == 6)
        {
            DataRate = StringValue("HtMcs6");
            datarate = 58.5;
        }
        else if (i == 7)
        {
            DataRate = StringValue("HtMcs7");
            datarate = 65;
        }
        else if (i == 8)
        {
            DataRate = StringValue("HtMcs0");
            datarate = 7.2;
        }
        else if (i == 9)
        {
            DataRate = StringValue("HtMcs1");
            datarate = 14.4;
        }
        else if (i == 10)
        {
            DataRate = StringValue("HtMcs2");
            datarate = 21.7;
        }
        else if (i == 11)
        {
            DataRate = StringValue("HtMcs3");
            datarate = 28.9;
        }
        else if (i == 12)
        {
            DataRate = StringValue("HtMcs4");
            datarate = 43.3;
        }
        else if (i == 13)
        {
            DataRate = StringValue("HtMcs5");
            datarate = 57.8;
        }
        else if (i == 14)
        {
            DataRate = StringValue("HtMcs6");
            datarate = 65;
        }
        else if (i == 15)
        {
            DataRate = StringValue("HtMcs7");
            datarate = 72.2;
        }
        else if (i == 16)
        {
            DataRate = StringValue("HtMcs0");
            datarate = 13.5;
        }
        else if (i == 17)
        {
            DataRate = StringValue("HtMcs1");
            datarate = 27;
        }
        else if (i == 18)
        {
            DataRate = StringValue("HtMcs2");
            datarate = 40.5;
        }
        else if (i == 19)
        {
            DataRate = StringValue("HtMcs3");
            datarate = 54;
        }
        else if (i == 20)
        {
            DataRate = StringValue("HtMcs4");
            datarate = 81;
        }
        else if (i == 21)
        {
            DataRate = StringValue("HtMcs5");
            datarate = 108;
        }
        else if (i == 22)
        {
            DataRate = StringValue("HtMcs6");
            datarate = 121.5;
        }
        else if (i == 23)
        {
            DataRate = StringValue("HtMcs7");
            datarate = 135;
        }
        else if (i == 24)
        {
            DataRate = StringValue("HtMcs0");
            datarate = 15;
        }
        else if (i == 25)
        {
            DataRate = StringValue("HtMcs1");
            datarate = 30;
        }
        else if (i == 26)
        {
            DataRate = StringValue("HtMcs2");
            datarate = 45;
        }
        else if (i == 27)
        {
            DataRate = StringValue("HtMcs3");
            datarate = 60;
        }
        else if (i == 28)
        {
            DataRate = StringValue("HtMcs4");
            datarate = 90;
        }
        else if (i == 29)
        {
            DataRate = StringValue("HtMcs5");
            datarate = 120;
        }
        else if (i == 30)
        {
            DataRate = StringValue("HtMcs6");
            datarate = 135;
        }
        else
        {
            DataRate = StringValue("HtMcs7");
            datarate = 150;
        }

        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode",
                                     DataRate,
                                     "ControlMode",
                                     DataRate);

        NetDeviceContainer staDevice;
        NetDeviceContainer apDevice;

        if (wifiType == "ns3::YansWifiPhy")
        {
            mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
            staDevice = wifi.Install(phy, mac, wifiStaNode);
            mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
            apDevice = wifi.Install(phy, mac, wifiApNode);
        }
        else if (wifiType == "ns3::SpectrumWifiPhy")
        {
            mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
            staDevice = wifi.Install(spectrumPhy, mac, wifiStaNode);
            mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
            apDevice = wifi.Install(spectrumPhy, mac, wifiApNode);
        }

        bool shortGuardIntervalSupported = (i > 7 && i <= 15) || (i > 23);
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/"
                    "ShortGuardIntervalSupported",
                    BooleanValue(shortGuardIntervalSupported));

        // mobility.
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(distance, 0.0, 0.0));
        positionAlloc->Add(Vector(distance, distance, 0.0));
        mobility.SetPositionAllocator(positionAlloc);

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

        mobility.Install(wifiApNode);
        mobility.Install(wifiStaNode);
        mobility.Install(interferingNode);

        /* Internet stack*/
        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNode);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staNodeInterface;
        Ipv4InterfaceContainer apNodeInterface;

        staNodeInterface = address.Assign(staDevice);
        apNodeInterface = address.Assign(apDevice);

        /* Setting applications */
        ApplicationContainer serverApp;
        if (udp)
        {
            // UDP flow
            uint16_t port = 9;
            UdpServerHelper server(port);
            serverApp = server.Install(wifiStaNode.Get(0));
            serverApp.Start(Seconds(0.0));
            serverApp.Stop(simulationTime + Seconds(1.0));
            const auto packetInterval = payloadSize * 8.0 / (datarate * 1e6);

            UdpClientHelper client(staNodeInterface.GetAddress(0), port);
            client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
            client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
            client.SetAttribute("PacketSize", UintegerValue(payloadSize));
            ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(simulationTime + Seconds(1.0));
        }
        else
        {
            // TCP flow
            uint16_t port = 50000;
            Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
            PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
            serverApp = packetSinkHelper.Install(wifiStaNode.Get(0));
            serverApp.Start(Seconds(0.0));
            serverApp.Stop(simulationTime + Seconds(1.0));

            OnOffHelper onoff("ns3::TcpSocketFactory", Ipv4Address::GetAny());
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
            onoff.SetAttribute("DataRate", DataRateValue(datarate * 1e6));
            AddressValue remoteAddress(InetSocketAddress(staNodeInterface.GetAddress(0), port));
            onoff.SetAttribute("Remote", remoteAddress);
            ApplicationContainer clientApp = onoff.Install(wifiApNode.Get(0));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(simulationTime + Seconds(1.0));
        }

        // Configure waveform generator
        Ptr<SpectrumValue> wgPsd =
            Create<SpectrumValue>(i <= 15 ? SpectrumModelWifi5180MHz : SpectrumModelWifi5190MHz);
        *wgPsd = waveformPower / 20e6; // PSD spread across 20 MHz
        NS_LOG_INFO("wgPsd : " << *wgPsd
                               << " integrated power: " << Integral(*(GetPointer(wgPsd))));

        if (wifiType == "ns3::SpectrumWifiPhy")
        {
            WaveformGeneratorHelper waveformGeneratorHelper;
            waveformGeneratorHelper.SetChannel(spectrumChannel);
            waveformGeneratorHelper.SetTxPowerSpectralDensity(wgPsd);

            waveformGeneratorHelper.SetPhyAttribute("Period", TimeValue(Seconds(0.0007)));
            waveformGeneratorHelper.SetPhyAttribute("DutyCycle", DoubleValue(1));
            NetDeviceContainer waveformGeneratorDevices =
                waveformGeneratorHelper.Install(interferingNode);

            Simulator::Schedule(Seconds(0.002),
                                &WaveformGenerator::Start,
                                waveformGeneratorDevices.Get(0)
                                    ->GetObject<NonCommunicatingNetDevice>()
                                    ->GetPhy()
                                    ->GetObject<WaveformGenerator>());
        }

        Config::ConnectWithoutContext("/NodeList/0/DeviceList/*/Phy/MonitorSnifferRx",
                                      MakeCallback(&MonitorSniffRx));

        if (enablePcap)
        {
            phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
            std::stringstream ss;
            ss << "wifi-spectrum-per-example-" << i;
            phy.EnablePcap(ss.str(), apDevice);
        }
        g_signalDbmAvg = 0;
        g_noiseDbmAvg = 0;
        g_samples = 0;

        // Make sure we are tuned to 5180 MHz; if not, the example will
        // not work properly
        Ptr<NetDevice> staDevicePtr = staDevice.Get(0);
        Ptr<WifiPhy> wifiPhyPtr = staDevicePtr->GetObject<WifiNetDevice>()->GetPhy();
        if (i <= 15)
        {
            NS_ABORT_MSG_IF(wifiPhyPtr->GetChannelWidth() != 20,
                            "Error: Channel width must be 20 MHz if MCS index <= 15");
            NS_ABORT_MSG_IF(
                wifiPhyPtr->GetFrequency() != 5180,
                "Error:  Wi-Fi nodes must be tuned to 5180 MHz to match the waveform generator");
        }
        else
        {
            NS_ABORT_MSG_IF(wifiPhyPtr->GetChannelWidth() != 40,
                            "Error: Channel width must be 40 MHz if MCS index > 15");
            NS_ABORT_MSG_IF(
                wifiPhyPtr->GetFrequency() != 5190,
                "Error:  Wi-Fi nodes must be tuned to 5190 MHz to match the waveform generator");
        }

        Simulator::Stop(simulationTime + Seconds(1.0));
        Simulator::Run();

        auto throughput = 0.0;
        auto totalPacketsThrough = 0.0;
        if (udp)
        {
            // UDP
            totalPacketsThrough = DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();
            throughput =
                totalPacketsThrough * payloadSize * 8 / simulationTime.GetMicroSeconds(); // Mbit/s
        }
        else
        {
            // TCP
            auto totalBytesRx = DynamicCast<PacketSink>(serverApp.Get(0))->GetTotalRx();
            totalPacketsThrough = totalBytesRx / tcpPacketSize;
            throughput = totalBytesRx * 8 / simulationTime.GetMicroSeconds(); // Mbit/s
        }
        std::cout << std::setw(5) << i << std::setw(6) << (i % 8) << std::setprecision(2)
                  << std::fixed << std::setw(10) << datarate << std::setw(12) << throughput
                  << std::setw(8) << totalPacketsThrough;
        if (totalPacketsThrough > 0)
        {
            std::cout << std::setw(12) << g_signalDbmAvg << std::setw(12) << g_noiseDbmAvg
                      << std::setw(12) << (g_signalDbmAvg - g_noiseDbmAvg) << std::endl;
        }
        else
        {
            std::cout << std::setw(12) << "N/A" << std::setw(12) << "N/A" << std::setw(12) << "N/A"
                      << std::endl;
        }
        Simulator::Destroy();
    }
    return 0;
}
