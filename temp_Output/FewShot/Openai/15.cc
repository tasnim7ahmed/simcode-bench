#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughputVsDistance");

struct SimResult {
    double distance;
    std::map<std::string, double> mcsThroughput;
};

class ThroughputCalculator : public Object
{
public:
    ThroughputCalculator() : m_bytes(0) {}
    void RxCallback (Ptr<const Packet> packet, const Address &)
    {
        m_bytes += packet->GetSize();
    }
    double CalculateThroughput(double simTime)
    {
        return (m_bytes * 8.0) / simTime / 1e6; // Mbps
    }
private:
    uint64_t m_bytes;
};

int main (int argc, char *argv[])
{
    // Simulation parameters with defaults
    double minDistance = 1.0;
    double maxDistance = 50.0;
    double distanceStep = 5.0;
    double simulationTime = 5.0; // seconds
    bool useUdp = true;
    std::string phyBand = "5GHz"; // "2.4GHz" or "5GHz"
    uint32_t channelWidth = 20; // MHz
    bool shortGuardInterval = false;
    bool greenfield = false;
    uint32_t maxMimoStreams = 4;
    std::string preambleStr = "ht-mixed";
    std::string plotTitle = "802.11n MIMO Throughput vs Distance";
    std::string outputFile = "wifi-mimo-throughput-vs-distance.plt";
    std::string legendPos = "top right";
    uint32_t tcpPacketSize = 1400;
    uint32_t udpPacketSize = 1400;
    uint32_t udpRateMbps = 200;
    bool doChannelBonding = false; // Channel width = 40MHz if true

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue ("distanceStep", "Step size for distance (m)", distanceStep);
    cmd.AddValue ("minDistance", "Minimum distance (m)", minDistance);
    cmd.AddValue ("maxDistance", "Maximum distance (m)", maxDistance);
    cmd.AddValue ("simTime", "Simulation time per point (s)", simulationTime);
    cmd.AddValue ("udp", "Use UDP (default true), set false to use TCP", useUdp);
    cmd.AddValue ("phyBand", "Frequency band: 2.4GHz or 5GHz", phyBand);
    cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
    cmd.AddValue ("shortGuardInterval", "Short guard interval (default false)", shortGuardInterval);
    cmd.AddValue ("greenfield", "Use Greenfield preamble (default false)", greenfield);
    cmd.AddValue ("maxMimoStreams", "Maximum number of MIMO streams (1-4)", maxMimoStreams);
    cmd.AddValue ("preamble", "Preamble: ht-mixed or ht-greenfield", preambleStr);
    cmd.AddValue ("channelBonding", "Enable channel bonding (40 MHz)", doChannelBonding);
    cmd.Parse(argc, argv);

    if (doChannelBonding) channelWidth = 40;

    // 802.11n MCS indices: 0-7 per spatial stream
    // For 1 stream: MCS 0-7; for 2: MCS 8-15; 3: 16-23; 4: 24-31
    // Let's use all 8 MCS per stream
    std::vector<uint8_t> mcsPerStream = {0,1,2,3,4,5,6,7};
    const uint32_t minStreams = 1;
    const uint32_t maxStreams = std::min((uint32_t)4, maxMimoStreams);

    // Prepare Gnuplot
    Gnuplot gnuplot(outputFile);
    gnuplot.SetTitle(plotTitle);
    gnuplot.SetTerminal("png");
    gnuplot.SetLegend("Distance (m)", "Throughput (Mbps)");
    gnuplot.AppendExtra("set grid");
    gnuplot.AppendExtra("set key " + legendPos);

    std::vector<Gnuplot2dDataset> datasets;

    // X (distance) axis values
    std::vector<double> distances;
    for (double d = minDistance; d <= maxDistance + 0.01; d += distanceStep)
        distances.push_back(d);

    // For each (streams, mcs) pair, simulate and record throughput per distance
    std::vector<std::pair<uint32_t,uint8_t>> configs;
    for(uint32_t nstreams = minStreams; nstreams <= maxStreams; ++nstreams)
        for(uint8_t mcs = 0; mcs < mcsPerStream.size(); ++mcs)
            configs.emplace_back(nstreams, mcs);

    std::vector<Gnuplot2dDataset> plotDatasets(configs.size());

    std::vector<std::vector<double>> throughputResults(configs.size(), std::vector<double>(distances.size(), 0.0));

    // Loop over distances
    for (size_t idist = 0; idist < distances.size(); ++idist)
    {
        double distance = distances[idist];
        NS_LOG_INFO("Simulating for distance " << distance << " m");

        // For each (streams, mcs)
        for (size_t cfgIdx = 0; cfgIdx < configs.size(); ++cfgIdx) {
            uint32_t nstreams = configs[cfgIdx].first;
            uint8_t mcsPerStreamIdx = configs[cfgIdx].second;
            uint8_t mcs = mcsPerStreamIdx + 8 * (nstreams-1);

            // Clean up between runs
            Config::Reset();
            SeedManager::SetSeed(1);
            SeedManager::SetRun(idist * configs.size() + cfgIdx);

            // Create nodes
            NodeContainer wifiStaNode;
            wifiStaNode.Create (1);
            NodeContainer wifiApNode;
            wifiApNode.Create (1);

            // Wifi channel
            YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
            channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
            channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                "ReferenceDistance", DoubleValue(1.0),
                "ReferenceLoss", DoubleValue (46.6777 - 20*std::log10(2.412)), // at 2.4GHz
                "Exponent", DoubleValue(3.0));
            Ptr<YansWifiChannel> wifiChannel = channel.Create();

            // Wifi physical layer
            YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
            phy.SetChannel(wifiChannel);
            phy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));
            phy.Set("ChannelWidth", UintegerValue(channelWidth));
            phy.Set("GreenfieldEnabled", BooleanValue(greenfield));

            if (phyBand == "2.4GHz") {
                phy.Set("Frequency", UintegerValue(2412));
                phy.Set("ChannelNumber", UintegerValue(1));
            } else {
                phy.Set("Frequency", UintegerValue(5180));
                phy.Set("ChannelNumber", UintegerValue(36));
            }

            // Wifi MAC and standard
            WifiHelper wifi;
            wifi.SetStandard(WIFI_STANDARD_80211n);

            // Select HT rate control and force MCS/streams
            std::ostringstream oss;
            oss << "HtMcs" << unsigned(mcs);
            wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                "DataMode", StringValue(oss.str()),
                "ControlMode", StringValue(oss.str()));

            Ssid ssid = Ssid ("mimo-ssid");

            WifiMacHelper mac;
            mac.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid),
                "ActiveProbing", BooleanValue (false));

            // Set up MIMO (Tx/Rx antennas and spatial streams)
            phy.Set("Antennas", UintegerValue(nstreams));
            phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(nstreams));
            phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(nstreams));

            NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

            mac.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid));
            NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

            // Mobility
            MobilityHelper mobility;
            mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
            mobility.Install (wifiStaNode);
            mobility.Install (wifiApNode);

            Ptr<MobilityModel> staMobility = wifiStaNode.Get(0)->GetObject<MobilityModel>();
            Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
            staMobility->SetPosition (Vector (distance, 0.0, 0.0));
            apMobility->SetPosition (Vector (0.0, 0.0, 0.0));

            // Internet stack
            InternetStackHelper stack;
            stack.Install (wifiApNode);
            stack.Install (wifiStaNode);

            Ipv4AddressHelper address;
            address.SetBase ("10.1.1.0", "255.255.255.0");
            Ipv4InterfaceContainer staIf = address.Assign (staDevice);
            Ipv4InterfaceContainer apIf = address.Assign (apDevice);

            // Applications: set up throughput monitor on receiver
            uint16_t port = 9;
            ApplicationContainer serverApp, clientApp;
            Ptr<ThroughputCalculator> calc = CreateObject<ThroughputCalculator>();

            if (useUdp) {
                UdpServerHelper server(port);
                serverApp = server.Install(wifiApNode.Get(0));
                serverApp.Start(Seconds(0.0));
                serverApp.Stop(Seconds(simulationTime+0.5));

                UdpClientHelper client(apIf.GetAddress(0), port);
                client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                client.SetAttribute("Interval", TimeValue (Seconds(0.00001)));
                client.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
                // Data rate is regulated by application (not enforced exactly), so overload application

                clientApp = client.Install(wifiStaNode.Get(0));
                clientApp.Start(Seconds(0.5));
                clientApp.Stop(Seconds(simulationTime+0.5));

                // Install throughput probe
                Ptr<Application> app = serverApp.Get(0);
                app->TraceConnectWithoutContext("Rx", MakeCallback(&ThroughputCalculator::RxCallback, calc));

            } else {
                // TCP -- sink on AP, bulk send on STA
                uint16_t tcpPort = 50000;
                Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
                PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", localAddress);
                serverApp = sinkHelper.Install(wifiApNode.Get(0));
                serverApp.Start(Seconds(0.0));
                serverApp.Stop(Seconds(simulationTime+0.5));

                BulkSendHelper bulkSender ("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), tcpPort));
                bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
                bulkSender.SetAttribute("SendSize", UintegerValue(tcpPacketSize));
                clientApp = bulkSender.Install (wifiStaNode.Get(0));
                clientApp.Start(Seconds(0.5));
                clientApp.Stop(Seconds(simulationTime+0.5));

                // Install throughput probe
                Ptr<Application> app = serverApp.Get(0);
                app->TraceConnectWithoutContext("Rx", MakeCallback(&ThroughputCalculator::RxCallback, calc));
            }

            // Run sim
            Simulator::Stop(Seconds(simulationTime+1));
            Simulator::Run();

            // Compute throughput
            double tp = calc->CalculateThroughput(simulationTime);
            throughputResults[cfgIdx][idist] = tp;

            Simulator::Destroy();
        }
    }

    // Prepare plot datasets
    for (size_t cfgIdx = 0; cfgIdx < configs.size(); ++cfgIdx) {
        uint32_t nstreams = configs[cfgIdx].first;
        uint8_t mcsPerStreamIdx = configs[cfgIdx].second;
        unsigned mcsOverall = mcsPerStreamIdx + 8 * (nstreams-1);

        std::ostringstream oss;
        oss << nstreams << "SS-MCS" << unsigned(mcsPerStreamIdx) << " (" << unsigned(mcsOverall) << ")";
        Gnuplot2dDataset dataset;
        dataset.SetTitle (oss.str());
        dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

        for(size_t idist=0;idist<distances.size();++idist) {
            dataset.Add(distances[idist], throughputResults[cfgIdx][idist]);
        }
        datasets.push_back(dataset);
    }
    for(auto &d: datasets) gnuplot.AddDataset (d);

    std::ofstream plotFile(outputFile);
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    std::cout << "Simulation completed. Gnuplot file generated: " << outputFile << std::endl;
    return 0;
}