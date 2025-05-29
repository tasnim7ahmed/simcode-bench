#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

static void
RxEvent(std::string context, Ptr<const Packet> pkt, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise)
{
    double rxRss = signalNoise.signal;
    double rxNoise = signalNoise.noise;
    double rxSnr = rxRss - rxNoise;
    NS_LOG_UNCOND("[" << Simulator::Now().GetSeconds() << "s] " << context
                      << " - RSS: " << rxRss << " dBm, Noise: " << rxNoise << " dBm, SNR: " << rxSnr << " dB, "
                      << "MCS: " << txVector.GetMode().GetMcsValue() << ", ChWidth: " << txVector.GetChannelWidth() << " MHz, SGI: " << (txVector.IsShortGuardInterval() ? "Yes" : "No"));
}

static void
PrintThroughput(Ptr<PacketSink> sink, double startTime)
{
    static uint64_t lastTotalRx = 0;
    Time now = Simulator::Now();
    double cur = (sink->GetTotalRx() - lastTotalRx) * 8.0 / 1e6; // Mbps over last second
    NS_LOG_UNCOND("[" << now.GetSeconds() << "s] Throughput: " << cur << " Mbps");
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(Seconds(1.0), &PrintThroughput, sink, startTime);
}

int main(int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 10.0;
    double distance = 5.0;
    uint32_t mcsIdx = 7;
    uint32_t chWidthIdx = 0;
    uint32_t giIdx = 0;
    double waveformPowerDbm = 20.0;
    std::string wifiType = "spectrum";
    std::string errorModel = "YansErrorRateModel";
    bool useUdp = true;
    bool enablePcap = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.AddValue("distance", "Distance between Wi-Fi nodes (m)", distance);
    cmd.AddValue("mcsIdx", "MCS index (0-31)", mcsIdx);
    cmd.AddValue("chWidthIdx", "Channel width index (0=20MHz, 1=40MHz, 2=80MHz, 3=160MHz)", chWidthIdx);
    cmd.AddValue("giIdx", "Guard interval index (0=long, 1=short)", giIdx);
    cmd.AddValue("waveformPowerDbm", "Waveform interference power in dBm", waveformPowerDbm);
    cmd.AddValue("wifiType", "Wi-Fi helper type: yans or spectrum", wifiType);
    cmd.AddValue("errorModel", "Error rate model (YansErrorRateModel/NistErrorRateModel)", errorModel);
    cmd.AddValue("useUdp", "Use UDP (default) or TCP (false)", useUdp);
    cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Channel width choices
    uint32_t chWidthTab[4] = {20, 40, 80, 160};
    uint32_t channelWidth = chWidthTab[std::min(chWidthIdx, 3u)];
    bool shortGuardInterval = (giIdx != 0);

    // Create nodes: STA, AP, interferer
    NodeContainer wifiStaNode, wifiApNode, interfererNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);
    interfererNode.Create(1);

    // Mobility for all nodes
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 1.0));           // AP at origin
    posAlloc->Add(Vector(distance, 0.0, 1.0));      // STA at X=distance
    posAlloc->Add(Vector(distance/2, 5.0, 1.0));    // Interferer (not colinear)
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);
    mobility.Install(interfererNode);

    // Setup Wi-Fi channel
    Ptr<WifiChannel> wifiChannel;
    Ptr<YansWifiChannel> yansChan;
    Ptr<MultiModelSpectrumChannel> spectrumChan;
    Ptr<YansWifiPhyHelper> yansPhyHelper;
    Ptr<SpectrumWifiPhyHelper> spectrumPhyHelper;
    NetDeviceContainer staDev, apDev;

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs" + std::to_string(mcsIdx)),
                                "ControlMode", StringValue("HtMcs0"));

    HtWifiMacHelper mac = HtWifiMacHelper::Default();
    Ssid ssid = Ssid("ns3-80211n");

    if (wifiType == "spectrum")
    {
        SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default();
        spectrumPhy.SetErrorRateModel(errorModel);
        spectrumChan = CreateObject<MultiModelSpectrumChannel>();
        Ptr<ConstantSpeedPropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
        spectrumChan->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
        spectrumChan->SetPropagationDelayModel(delay);
        spectrumPhy.SetChannel(spectrumChan);
        spectrumPhy.Set("ChannelWidth", UintegerValue(channelWidth));
        spectrumPhy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));
        spectrumPhy.Set("TxPowerStart", DoubleValue(20.0));
        spectrumPhy.Set("TxPowerEnd", DoubleValue(20.0));
        spectrumPhy.Set("RxGain", DoubleValue(0.0));
        spectrumPhy.Set("TxGain", DoubleValue(0.0));
        spectrumPhy.Set("Frequency", UintegerValue(5180)); // channel 36, 5 GHz
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
        staDev = wifi.Install(spectrumPhy, mac, wifiStaNode);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
        apDev = wifi.Install(spectrumPhy, mac, wifiApNode);

        spectrumPhyHelper = CreateObject<SpectrumWifiPhyHelper>(spectrumPhy);
    }
    else
    {
        YansWifiPhyHelper yansPhy = YansWifiPhyHelper::Default();
        yansChan = CreateObject<YansWifiChannel>();
        yansChan->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
        yansChan->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
        yansPhy.SetChannel(yansChan);
        yansPhy.Set("ChannelWidth", UintegerValue(channelWidth));
        yansPhy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));
        yansPhy.Set("TxPowerStart", DoubleValue(20.0));
        yansPhy.Set("TxPowerEnd", DoubleValue(20.0));
        yansPhy.Set("RxGain", DoubleValue(0.0));
        yansPhy.Set("TxGain", DoubleValue(0.0));
        yansPhy.Set("Frequency", UintegerValue(5180));
        yansPhy.SetErrorRateModel(errorModel);
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
        staDev = wifi.Install(yansPhy, mac, wifiStaNode);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
        apDev = wifi.Install(yansPhy, mac, wifiApNode);

        yansPhyHelper = CreateObject<YansWifiPhyHelper>(yansPhy);
    }

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf, apIf;
    NetDeviceContainer totalDevs;
    totalDevs.Add(staDev);
    totalDevs.Add(apDev);

    staIf = address.Assign(staDev);
    apIf = address.Assign(apDev);

    // Interferer setup (uses SpectrumWaveformGenerator or a raw transmitter)
    Ptr<SpectrumChannel> interfererChan;
    if (wifiType == "spectrum")
    {
        interfererChan = spectrumChan;
    }
    else
    {
        interfererChan = yansChan->GetSpectrumChannel();
    }
    Ptr<SpectrumWaveformGenerator> waveformGen = CreateObject<SpectrumWaveformGenerator>();
    waveformGen->SetChannel(interfererChan);
    waveformGen->SetTxPowerDbm(waveformPowerDbm);
    waveformGen->SetFrequency(5180e6);
    waveformGen->SetChannelWidth(channelWidth);

    interfererNode.Get(0)->AggregateObject(waveformGen);

    // Install "OnOff" traffic from STA to AP
    uint16_t port = 5001;
    ApplicationContainer clientApps, serverApps;
    if (useUdp)
    {
        // UDP
        UdpServerHelper udpServer(port);
        serverApps = udpServer.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.1));
        serverApps.Stop(Seconds(simTime + 1.0));

        UdpClientHelper udpClient(apIf.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1472));
        clientApps = udpClient.Install(wifiStaNode.Get(0));
        clientApps.Start(Seconds(0.5));
        clientApps.Stop(Seconds(simTime));
    }
    else
    {
        // TCP
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApps = packetSinkHelper.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.1));
        serverApps.Stop(Seconds(simTime + 1.0));

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("50Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(1472));
        clientApps = onoff.Install(wifiStaNode.Get(0));
        clientApps.Start(Seconds(0.5));
        clientApps.Stop(Seconds(simTime));
    }

    // PacketCapture
    if (enablePcap)
    {
        if (wifiType == "spectrum")
        {
            SpectrumWifiPhyHelper::Default().EnablePcapAll("wifi-interference-spectrum", true);
        }
        else
        {
            YansWifiPhyHelper::Default().EnablePcapAll("wifi-interference-yans", true);
        }
    }

    // Tracing -- connect to RX events for logs
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback(&RxEvent));

    // Print info about transmission rates, etc.
    double startTime = 1.0;
    Ptr<PacketSink> sink;
    if (useUdp)
    {
        sink = DynamicCast<PacketSink>(serverApps.Get(0));
    }
    else
    {
        sink = DynamicCast<PacketSink>(serverApps.Get(0));
    }
    Simulator::Schedule(Seconds(startTime), &PrintThroughput, sink, startTime);

    // (Optional) FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Interferer waveform transmission
    waveformGen->Start();
    Simulator::Schedule(Seconds(simTime), &SpectrumWaveformGenerator::Stop, waveformGen);

    NS_LOG_UNCOND("Starting simulation with:"
                  << " MCS=HtMcs" << mcsIdx
                  << " ChannelWidth=" << channelWidth << "MHz"
                  << " ShortGI=" << (shortGuardInterval ? "Yes" : "No")
                  << " WiFiType=" << wifiType
                  << " ErrorModel=" << errorModel
                  << " WaveformPower=" << waveformPowerDbm << "dBm"
                  << " Traffic=" << (useUdp ? "UDP" : "TCP"));

    Simulator::Stop(Seconds(simTime+1.0));
    Simulator::Run();

    // Show final throughput summary for FlowMonitor
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        NS_LOG_UNCOND("Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n"
            "\tTxBytes:   " << it->second.txBytes << "\n"
            "\tRxBytes:   " << it->second.rxBytes << "\n"
            "\tThroughput: " << it->second.rxBytes * 8.0 / (simTime * 1e6) << " Mbps");
    }

    Simulator::Destroy();
    return 0;
}