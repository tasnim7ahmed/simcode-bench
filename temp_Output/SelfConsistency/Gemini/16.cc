#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExperiment");

class Experiment {
public:
    Experiment(std::string wifiRate, std::string rtsCtsThreshold, std::string manager)
        : wifiRate(wifiRate), rtsCtsThreshold(rtsCtsThreshold), manager(manager) {}

    void Run(int simulationTime, double distance) {
        CreateNodes();
        InstallWifiDevices();
        InstallMobilityModel(distance);
        InstallInternetStack();
        InstallApplications(simulationTime);
        SetupPacketReception();
        Simulator::Stop(Seconds(simulationTime));
        Simulator::Run();
        GenerateGnuplotData();
        Simulator::Destroy();
    }

    Vector GetPosition(Ptr<Node> node) const {
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        return mobility->GetPosition();
    }

    void MoveNode(Ptr<Node> node, Vector position) {
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        mobility->SetPosition(position);
    }

    void ReceivePacket(Ptr<Socket> socket) {
        Address from;
        Ptr<Packet> packet = socket->RecvFrom(from);
        NS_LOG_INFO("Received packet!");
    }

    void SetupPacketReception() {
        PacketSocketHelper packetSocketHelper;
        packetSocketHelper.Install(nodes.Get(1));
        Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(1), PacketSocketFactory::GetTypeId());
        socket->Bind();
        socket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
    }

private:
    void CreateNodes() {
        nodes.Create(2);
    }

    void InstallWifiDevices() {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        Ssid ssid = Ssid("adhoc-wifi");
        mac.SetParam("ssid", SsidValue(ssid));

        wifi.SetRemoteStationManager(manager,
                                     "DataMode", StringValue(wifiRate),
                                     "RtsCtsThreshold", StringValue(rtsCtsThreshold));

        devices = wifi.Install(phy, mac, nodes);
    }

    void InstallMobilityModel(double distance) {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(distance),
                                      "DeltaY", DoubleValue(distance),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);
    }

    void InstallInternetStack() {
        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = ipv4.Assign(devices);
    }

    void InstallApplications(int simulationTime) {
        uint16_t port = 9;  // Discard port (RFC 863)

        OnOffHelper onoff("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(interfaces.GetAddress(1), port)));
        onoff.SetConstantRate(DataRate("500Kbps"));

        ApplicationContainer app = onoff.Install(nodes.Get(0));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime));

        PacketSinkHelper sink("ns3::UdpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port));
        app = sink.Install(nodes.Get(1));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime));
    }

    void GenerateGnuplotData() {
        std::string fileNameWithExtension = wifiRate + "-" + manager + ".plt";
        Gnuplot2dDataset dataset;
        dataset.SetTitle(wifiRate + "-" + manager);
        dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

        Vector positionNode0 = GetPosition(nodes.Get(0));
        Vector positionNode1 = GetPosition(nodes.Get(1));
        dataset.Add(positionNode0.x, positionNode0.y);
        dataset.Add(positionNode1.x, positionNode1.y);

        Gnuplot plot(fileNameWithExtension);
        plot.AddDataset(dataset);
        plot.GenerateOutput(std::cout);
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    std::string wifiRate;
    std::string rtsCtsThreshold;
    std::string manager;
};

int main(int argc, char* argv[]) {
    LogComponentEnable("AdhocWifiExperiment", LOG_LEVEL_INFO);

    std::string wifiRate = "DsssRate1Mbps";
    std::string rtsCtsThreshold = "2000";
    std::string manager = "ns3::AarfWifiManager";
    int simulationTime = 10;
    double distance = 10.0;

    CommandLine cmd;
    cmd.AddValue("wifiRate", "Wifi data rate", wifiRate);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold", rtsCtsThreshold);
    cmd.AddValue("manager", "Wifi manager", manager);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.Parse(argc, argv);

    Experiment experiment(wifiRate, rtsCtsThreshold, manager);
    experiment.Run(simulationTime, distance);

    return 0;
}