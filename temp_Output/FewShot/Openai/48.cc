#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsTest");

// Function to log PHY events (signal power, noise, SNR)
class PhyStats : public Object
{
public:
    PhyStats() = default;
    void PhyRxBegin(Ptr<const Packet> packet, double snr, WifiMode mode, WifiPreamble preamble);
    void Report();

    void SetCurrentParams(uint8_t mcs, uint8_t chWidth, bool shortGi)
    {
        m_currentMcs = mcs;
        m_currentChWidth = chWidth;
        m_currentShortGi = shortGi;
        m_receivedCount = 0;
        m_snrSum = 0.0;
        m_signalSum = 0.0;
        m_noiseSum = 0.0;
    }

    struct ReportItem
    {
        uint8_t mcs;
        uint8_t chWidth;
        bool shortGi;
        uint32_t rxCount;
        double avgSnr;
        double avgSignal;
        double avgNoise;
    };

    std::vector<ReportItem> m_reportData;

private:
    uint8_t m_currentMcs;
    uint8_t m_currentChWidth;
    bool m_currentShortGi;
    uint32_t m_receivedCount = 0;
    double m_snrSum = 0.0;
    double m_signalSum = 0.0;
    double m_noiseSum = 0.0;
};

void
PhyStats::PhyRxBegin(Ptr<const Packet>, double snr, WifiMode, WifiPreamble)
{
    double signalDbm = 10*log10(1e-9); // Placeholders, real data is set from PHY events
    double noiseDbm = 10*log10(1e-9);
    m_snrSum += snr;
    m_signalSum += signalDbm;
    m_noiseSum += noiseDbm;
    m_receivedCount++;
}

void
PhyStats::Report()
{
    ReportItem item;
    item.mcs = m_currentMcs;
    item.chWidth = m_currentChWidth;
    item.shortGi = m_currentShortGi;
    item.rxCount = m_receivedCount;
    item.avgSnr = (m_receivedCount > 0) ? m_snrSum/m_receivedCount : 0;
    item.avgSignal = (m_receivedCount > 0) ? m_signalSum/m_receivedCount : 0;
    item.avgNoise = (m_receivedCount > 0) ? m_noiseSum/m_receivedCount : 0;
    m_reportData.push_back(item);
}

void PrintReport(std::vector<PhyStats::ReportItem>& reportData)
{
    std::cout << std::setw(5) << "MCS"
              << std::setw(8) << "ChWidth"
              << std::setw(10) << "ShortGI"
              << std::setw(8) << "RxCnt"
              << std::setw(12) << "AvgSNR"
              << std::setw(12) << "AvgSignal"
              << std::setw(12) << "AvgNoise"
              << std::setw(12) << "Thr(Mbps)" << std::endl;
    for(const auto& item : reportData)
    {
        std::cout << std::setw(5) << int(item.mcs)
                  << std::setw(8) << int(item.chWidth)
                  << std::setw(10) << (item.shortGi ? "Yes" : "No")
                  << std::setw(8) << item.rxCount
                  << std::setw(12) << std::fixed << std::setprecision(2) << item.avgSnr
                  << std::setw(12) << item.avgSignal
                  << std::setw(12) << item.avgNoise
                  << std::setw(12) << "?" // Throughput dummy, set below
                  << std::endl;
    }
}

struct Args
{
    double simTime = 10.0;
    double distance = 10.0;
    uint8_t phyMode = 0; // 0: Yans, 1: Spectrum
    bool udp = true;
    bool pcap = false;
    std::vector<uint8_t> mcsList = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    std::vector<uint8_t> chWidthList = {20, 40};
    std::vector<bool> giList = {false, true};
};

void ParseArgs(int argc, char* argv[], Args& args)
{
    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time (s)", args.simTime);
    cmd.AddValue("distance", "Distance between nodes (m)", args.distance);
    cmd.AddValue("phyMode", "PHY model: 0=Yans, 1=Spectrum", args.phyMode);
    cmd.AddValue("udp", "Set 1 for UDP, 0 for TCP", args.udp);
    cmd.AddValue("pcap", "Enable PCAP tracing", args.pcap);
    cmd.Parse(argc, argv);
}

int main(int argc, char* argv[])
{
    Args args;
    ParseArgs(argc, argv, args);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Static Wi-Fi parameters
    uint16_t port = 5000;
    double frequency = 5.0; // 5GHz
    uint32_t payloadSize = 1472;

    // PHY stats collector
    Ptr<PhyStats> phyStats = CreateObject<PhyStats>();

    // Start iterations
    std::vector<double> throughputResults;
    std::vector<PhyStats::ReportItem> phyReport;

    for(uint8_t chWidth : args.chWidthList)
    {
        for(uint8_t mcs : args.mcsList)
        {
            for(bool shortGi : args.giList)
            {
                // Reset stats
                phyStats->SetCurrentParams(mcs, chWidth, shortGi);

                // Create nodes
                NodeContainer staNode, apNode;
                staNode.Create(1);
                apNode.Create(1);

                // Wi-Fi PHY/Channel setup
                Ptr<WifiPhy> phy;
                Ptr<WifiChannel> channel;
                SpectrumWifiPhyHelper spectrumPhy;
                YansWifiPhyHelper yansPhy = YansWifiPhyHelper::Default();
                YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default();
                SpectrumChannelHelper spectrumChannel = SpectrumChannelHelper::Default();
                if(args.phyMode == 1)
                {
                    spectrumChannel.SetChannel("ns3::MultiModelSpectrumChannel");
                    phy = spectrumPhy.Create(spectrumChannel.Create());
                }
                else
                {
                    channel = yansChannel.Create();
                    phy = yansPhy.Create(channel);
                }
                // Abstract over both PHYs
                WifiHelper wifi;
                wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
                WifiMacHelper mac;
                Ssid ssid = Ssid("ns3-wifi-mcs");
                NetDeviceContainer staDevice, apDevice;

                // Set MAC and DCF/MAC parameters
                mac.SetType("ns3::StaWifiMac",
                            "Ssid", SsidValue(ssid),
                            "ActiveProbing", BooleanValue(false));
                staDevice = wifi.Install(phy, mac, staNode);

                mac.SetType("ns3::ApWifiMac",
                            "Ssid", SsidValue(ssid));
                apDevice = wifi.Install(phy, mac, apNode);

                // PHY/Channel parameterization
                phy->SetChannelWidth(chWidth);
                phy->SetShortGuardInterval(shortGi);
                phy->SetFrequency(frequency * 1000); // Unit: MHz

                // Fixed MCS
                std::ostringstream mcsStr;
                mcsStr << "HtMcs" << int(mcs);
                wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                             "DataMode", StringValue(mcsStr.str()),
                                             "ControlMode", StringValue(mcsStr.str()));

                // Mobility: fixed positions at desired distance
                MobilityHelper mobility;
                Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
                posAlloc->Add(Vector(0.0, 0.0, 1.5));
                posAlloc->Add(Vector(args.distance, 0.0, 1.5));
                mobility.SetPositionAllocator(posAlloc);
                mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                mobility.Install(staNode);
                mobility.Install(apNode);

                // Internet stack
                InternetStackHelper stack;
                stack.Install(apNode);
                stack.Install(staNode);

                // IP assignment
                Ipv4AddressHelper addr;
                addr.SetBase("192.168.1.0", "255.255.255.0");
                Ipv4InterfaceContainer apIf = addr.Assign(apDevice);
                Ipv4InterfaceContainer staIf = addr.Assign(staDevice);

                // Application layer
                ApplicationContainer clientApp, serverApp;
                uint32_t maxBytes = 0; // Unlimited by default

                if(args.udp)
                {
                    UdpServerHelper server(port);
                    serverApp = server.Install(apNode);
                    serverApp.Start(Seconds(0.0));
                    serverApp.Stop(Seconds(args.simTime));

                    UdpClientHelper client(apIf.GetAddress(0), port);
                    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
                    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
                    clientApp = client.Install(staNode);
                    clientApp.Start(Seconds(1.0));
                    clientApp.Stop(Seconds(args.simTime));
                }
                else
                {
                    uint64_t sinkTotalRx = 0;
                    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                    serverApp = sink.Install(apNode);
                    serverApp.Start(Seconds(0.0));
                    serverApp.Stop(Seconds(args.simTime));

                    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port));
                    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
                    source.SetAttribute("SendSize", UintegerValue(payloadSize));
                    clientApp = source.Install(staNode);
                    clientApp.Start(Seconds(1.0));
                    clientApp.Stop(Seconds(args.simTime));
                }

                // Enable Packet capture if needed
                if(args.pcap)
                {
                    if(args.phyMode == 0)
                        yansPhy.EnablePcap("wifi-mcs-yans-ap", apDevice.Get(0));
                }

                // Flow monitor to compute throughput
                FlowMonitorHelper flowmon;
                Ptr<FlowMonitor> monitor = flowmon.InstallAll();

                // Connect PHY traces
                Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/Phy/MonitorSnifferRx",
                    MakeCallback([phyStats](Ptr<const Packet>, uint16_t freq, WifiTxVector txVector, SignalNoiseDbm snrDbm, uint16_t){
                        // freq, txVector etc can be used for more detail, but log S, N, SNR
                        phyStats->m_signalSum += snrDbm.signal;
                        phyStats->m_noiseSum  += snrDbm.noise;
                        phyStats->m_snrSum    += (snrDbm.signal - snrDbm.noise);
                        phyStats->m_receivedCount++;
                    }));

                Simulator::Stop(Seconds(args.simTime));
                Simulator::Run();

                // Calculate throughput
                double throughput = 0.0;
                monitor->CheckForLostPackets();
                std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
                for(auto it = stats.begin(); it != stats.end(); ++it)
                {
                    if(it->second.rxPackets > 0)
                    {
                        double bytesRx = it->second.rxBytes * 8.0; // bits
                        double dur = it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds();
                        throughput = (dur > 0) ? bytesRx / dur / 1e6 : 0.0; // in Mbps
                        break;
                    }
                }
                throughputResults.push_back(throughput);

                // Record PHY stats
                phyStats->Report();
                Simulator::Destroy();
            }
        }
    }

    // Print report
    // Overwrite throughput values in report
    for(size_t i = 0; i < phyStats->m_reportData.size(); ++i)
    {
        std::cout << std::setw(5) << int(phyStats->m_reportData[i].mcs)
                  << std::setw(8) << int(phyStats->m_reportData[i].chWidth)
                  << std::setw(10) << (phyStats->m_reportData[i].shortGi ? "Yes" : "No")
                  << std::setw(8) << phyStats->m_reportData[i].rxCount
                  << std::setw(12) << std::fixed << std::setprecision(2) << phyStats->m_reportData[i].avgSnr
                  << std::setw(12) << phyStats->m_reportData[i].avgSignal
                  << std::setw(12) << phyStats->m_reportData[i].avgNoise
                  << std::setw(12) << std::fixed << std::setprecision(3)
                  << (i < throughputResults.size() ? throughputResults[i] : 0.0)
                  << std::endl;
    }

    return 0;
}