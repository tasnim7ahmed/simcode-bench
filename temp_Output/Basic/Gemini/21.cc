#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiClearChannelExperiment");

class Experiment {
public:
    Experiment(double simulationTime, std::string experimentName)
        : m_simulationTime(simulationTime), m_experimentName(experimentName) {}

    void Setup(DataRate dataRate) {
        m_dataRate = dataRate;

        NodeContainer staNodes;
        staNodes.Create(1);

        NodeContainer apNodes;
        apNodes.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.Set("RxGain", DoubleValue(-10));
        wifiPhy.Set("TxGain", DoubleValue(0));
        wifiPhy.Set("CcaEdThreshold", DoubleValue(-85));
        wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-85));

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelay");
        wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("wifi-clear-channel");
        wifiMac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));

        NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

        wifiMac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid),
                        "BeaconInterval", TimeValue(Seconds(0.1)));

        NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNodes);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(staNodes);
        mobility.Install(apNodes);

        InternetStackHelper stack;
        stack.Install(staNodes);
        stack.Install(apNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staNodeInterface = address.Assign(staDevices);
        Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(m_simulationTime - 1));

        UdpEchoClientHelper echoClient(apNodeInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(m_simulationTime - 2));

        m_staNode = staNodes.Get(0);
        m_apNode = apNodes.Get(0);

        m_staInterface = staNodeInterface.Get(0);
    }

    void Run() {
        Simulator::Stop(Seconds(m_simulationTime));
        Simulator::Run();
        Simulator::Destroy();
    }

    int GetTotalPacketsReceived() {
        Ptr<Ipv4L3Protocol> ipv4 = m_apNode->GetObject<Ipv4L3Protocol>(0);
        if (ipv4) {
            Ipv4RoutingTableEntry entry;
            if (ipv4->GetRoutingTable()->LookupRoute(m_staInterface.GetAddress(0), entry)) {
              return entry.GetForwardingPacketsReceived();
            }
        }
        return 0;
    }

    double GetRss()
    {
      double rss = 0.0;
      Ptr<NetDevice> nd = m_staNode->GetDevice(0);
      Ptr<WifiNetDevice> wnd = nd->GetObject<WifiNetDevice>();
      if (wnd != NULL)
      {
        WifiPhyStateHelper phyState = wnd->GetPhy()->GetState();
        rss = phyState.GetLastRss();
      }

      return rss;
    }


    void CollectResults(std::vector<std::pair<double, int>>& results) {
      double rss = GetRss();
      int packetsReceived = GetTotalPacketsReceived();
      results.push_back(std::make_pair(rss, packetsReceived));
    }

    void GenerateGnuplotScript(const std::vector<std::pair<double, int>>& results) {
        std::string dataFilename = m_experimentName + ".dat";
        std::string plotFilename = m_experimentName + ".eps";
        std::string gnuplotFilename = m_experimentName + ".plt";

        std::ofstream dataFile(dataFilename.c_str());
        for (const auto& result : results) {
            dataFile << result.first << " " << result.second << std::endl;
        }
        dataFile.close();

        std::ofstream plotFile(gnuplotFilename.c_str());
        plotFile << "set terminal postscript eps enhanced color font 'Helvetica,10'" << std::endl;
        plotFile << "set output '" << plotFilename << "'" << std::endl;
        plotFile << "set title '" << m_experimentName << "'" << std::endl;
        plotFile << "set xlabel 'RSS (dBm)'" << std::endl;
        plotFile << "set ylabel 'Packets Received'" << std::endl;
        plotFile << "plot '" << dataFilename << "' with linespoints title 'Data'" << std::endl;
        plotFile.close();

        std::string command = "gnuplot " + gnuplotFilename;
        system(command.c_str());
    }

private:
    double m_simulationTime;
    std::string m_experimentName;
    DataRate m_dataRate;
    Ptr<Node> m_staNode;
    Ptr<Node> m_apNode;
    Ipv4Interface m_staInterface;
};

int main(int argc, char* argv[]) {
    LogComponentEnable("WifiClearChannelExperiment", LOG_LEVEL_INFO);

    double simulationTime = 10.0;

    std::vector<DataRate> dataRates = {
        DataRate("1Mb/s"),
        DataRate("2Mb/s"),
        DataRate("5.5Mb/s"),
        DataRate("11Mb/s")
    };

    std::vector<std::pair<double, int>> allResults;

    for (const auto& dataRate : dataRates) {
        std::string experimentName = "WifiClearChannel_" + dataRate.ToString();
        Experiment experiment(simulationTime, experimentName);
        experiment.Setup(dataRate);
        experiment.Run();

        std::vector<std::pair<double, int>> results;
        experiment.CollectResults(results);
        experiment.GenerateGnuplotScript(results);

        allResults.insert(allResults.end(), results.begin(), results.end());
    }

    return 0;
}