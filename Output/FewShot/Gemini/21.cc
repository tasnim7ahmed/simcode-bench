#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment {
public:
    Experiment(double distance, std::string rate, std::string phyMode)
        : m_distance(distance), m_rate(rate), m_phyMode(phyMode) {}

    void Run() {
        CreateNodes();
        InstallWifi();
        InstallMobility();
        InstallInternetStack();
        InstallApplications();
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        GenerateGnuplotData();
        Simulator::Destroy();
    }

private:
    void CreateNodes() {
        nodes.Create(2);
        staNode = nodes.Get(0);
        apNode = nodes.Get(1);
    }

    void InstallWifi() {
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                          "DataMode", StringValue(m_phyMode),
                                          "ControlMode", StringValue(m_phyMode));

        Ssid ssid = Ssid("wifi-experiment");
        WifiMacHelper mac;

        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

        NetDeviceContainer staDevice = wifiHelper.Install(phy, mac, staNode);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(1.0)));

        NetDeviceContainer apDevice = wifiHelper.Install(phy, mac, apNode);

        devices.Add(staDevice);
        devices.Add(apDevice);
    }

    void InstallMobility() {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        Ptr<Node> ap = nodes.Get(1);
        Ptr<MobilityModel> apModel = ap->GetObject<MobilityModel>();
        apModel->SetPosition(Vector(m_distance, 0, 0));
    }

    void InstallInternetStack() {
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);
    }

    void InstallApplications() {
        UdpClientHelper client(interfaces.GetAddress(1), 9);
        client.SetAttribute("Interval", TimeValue(Time("0.00001")));
        client.SetAttribute("MaxPackets", UintegerValue(10000));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(staNode);
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(9.0));

        UdpServerHelper server(9);
        ApplicationContainer serverApps = server.Install(apNode);
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(9.0));

        serverApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&Experiment::ReceivePacket, this));
    }

    static void ReceivePacket(Ptr<const Packet> p) {
        receivedPackets++;
    }

    void GenerateGnuplotData() {
        std::string filename = "wifi-rssi.dat";
        std::ofstream os(filename.c_str());
        os << m_distance << " " << receivedPackets << std::endl;
        os.close();

        std::string plotFilename = "wifi-rssi.plt";
        std::string graphicsFilename = "wifi-rssi.eps";
        std::string plotTitle = "Wifi RSSI vs. Packets Received";

        Gnuplot plot(graphicsFilename);
        plot.SetTitle(plotTitle);
        plot.SetLegend("Distance (m)", "Packets Received");
        plot.AddDataset(filename, "Distance", "Packets");
        plot.GenerateOutput(plotFilename);
    }

private:
    NodeContainer nodes;
    Ptr<Node> staNode, apNode;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    WifiHelper wifi;
    double m_distance;
    std::string m_rate;
    std::string m_phyMode;
    static uint32_t receivedPackets;
};

uint32_t Experiment::receivedPackets = 0;

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    std::vector<std::string> rates = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};

    for (double distance = 1; distance <= 20; distance += 1) {
        for (const auto& rate : rates) {
            Experiment experiment(distance, rate, rate);
            Experiment::receivedPackets = 0;
            experiment.Run();
        }
    }
    return 0;
}