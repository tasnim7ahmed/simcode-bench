#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment {
public:
    Experiment();
    void Setup(uint32_t nodeCount, WifiMode mode, double distance);
    void Run();
    void GenerateTraffic();
    void PacketRcvd(Ptr<const Packet> packet, const Address &from);

private:
    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;
    uint32_t m_receivedPackets;
};

Experiment::Experiment() : m_receivedPackets(0) {}

void Experiment::Setup(uint32_t nodeCount, WifiMode mode, double distance) {
    m_nodes.Create(nodeCount);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", WifiModeValue(mode));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    phy.Set("TxPowerStart", DoubleValue(16.0));
    phy.Set("TxPowerEnd", DoubleValue(16.0));
    phy.Set("RxGain", DoubleValue(0.0));

    m_devices = wifi.Install(phy, m_nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(nodeCount),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_nodes);

    InternetStackHelper stack;
    stack.Install(m_nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    m_interfaces = address.Assign(m_devices);

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxEndOk", MakeCallback(&Experiment::PacketRcvd, this));
}

void Experiment::Run() {
    GenerateTraffic();
    Simulator::Run();
    Simulator::Destroy();
}

void Experiment::GenerateTraffic() {
    UdpClientHelper udpClient(m_interfaces.GetAddress(1), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(m_nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(5.0));

    UdpServerHelper udpServer(9);
    ApplicationContainer serverApp = udpServer.Install(m_nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(5.0));
}

void Experiment::PacketRcvd(Ptr<const Packet> packet, const Address &from) {
    m_receivedPackets++;
}

int main(int argc, char *argv[]) {
    std::vector<WifiMode> modes = {
        WifiMode("DsssRate1Mbps"),
        WifiMode("DsssRate2Mbps"),
        WifiMode("DsssRate5_5Mbps"),
        WifiMode("DsssRate11Mbps")
    };

    GnuplotDataset dataset("RSS vs Packets Received");

    for (const auto &mode : modes) {
        double distance = 10.0; // Start with minimal interference
        while (distance <= 100.0) {
            Experiment experiment;
            experiment.Setup(2, mode, distance);
            experiment.Run();

            double rssi = - (46 + 20 * log10(distance)); // Simple free space path loss approximation
            dataset.Add(rssi, experiment.m_receivedPackets);

            distance += 10.0;
        }
    }

    Gnuplot plot("wifi_rss_plot.eps");
    plot.SetTerminal("postscript eps color enhanced");
    plot.SetTitle("RSS vs Packets Received (802.11b)");
    plot.SetLegend("Received Signal Strength (dBm)", "Packets Received");
    plot.AddDataset(dataset);

    std::ofstream plotFile("wifi_rss_plot.plt");
    plot.GenerateOutput(plotFile);

    return 0;
}