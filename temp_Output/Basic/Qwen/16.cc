#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment");

class Experiment {
public:
    Experiment();
    ~Experiment();

    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
    void Move(Ptr<Node> node, Vector newPosition, double seconds);
    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReception(Ptr<Node> node);

    void Run(double distance, std::string rate, std::string manager);

private:
    Gnuplot2dDataset m_dataset;
};

Experiment::Experiment() {
    m_dataset = Gnuplot2dDataset("Throughput");
}

Experiment::~Experiment() {}

void Experiment::SetPosition(Ptr<Node> node, Vector position) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    mobility->SetPosition(position);
}

Vector Experiment::GetPosition(Ptr<Node> node) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    return mobility->GetPosition();
}

void Experiment::Move(Ptr<Node> node, Vector newPosition, double seconds) {
    Simulator::Schedule(Seconds(seconds), &MobilityModel::SetPosition, node->GetObject<MobilityModel>(), newPosition);
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(From))) {
        NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
}

void Experiment::SetupPacketReception(Ptr<Node> node) {
    TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(node, tid);
    PacketSocketAddress local = PacketSocketAddress(Mac48Address::GetBroadcast(), 0, 0);
    recvSink->Bind(local);
    recvSink->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::Run(double distance, std::string rate, std::string manager) {
    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager(manager, "DataMode", StringValue(rate), "ControlMode", StringValue(rate));

    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    PacketSocketHelper packetSocket;
    packetSocket.Install(nodes);

    SetupPacketReception(nodes.Get(1));

    OnOffHelper onoff("ns3::PacketSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue(rate));
    onoff.SetAttribute("PacketSize", UintegerValue(1000));

    Address dest = PacketSocketAddress(Mac48Address::GetBroadcast(), 1, 0);
    onoff.SetAttribute("Remote", AddressValue(dest));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/Tx", MakeCallback([](Ptr<const Packet> p) {
        static uint64_t totalTx = 0;
        totalTx += p->GetSize();
    }));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback([distance](Ptr<const Packet> p, const Address&) {
        static uint64_t totalRx = 0;
        totalRx += p->GetSize();
        double throughput = (totalRx * 8) / (Simulator::Now().GetSeconds() * 1000000); // Mbps
        m_dataset.Add(distance, throughput);
    }));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    std::vector<std::pair<std::string, std::string>> configs = {
        {"DcaTxop", "OfdmRate6Mbps"},
        {"DcaTxop", "OfdmRate54Mbps"},
        {"ConstantRateManager", "OfdmRate24Mbps"}
    };

    for (auto& config : configs) {
        std::string manager = config.first;
        std::string rate = config.second;

        std::ostringstream oss;
        oss << "experiment-" << manager << "-" << rate << ".plt";
        Gnuplot plot(oss.str());
        plot.SetTitle(manager + " - " + rate);
        plot.SetTerminal("png");
        plot.SetOutputFileNameWithoutExtension("throughput-" + manager + "-" + rate);
        plot.AddDataset(Gnuplot2dDataset());

        for (double distance = 10; distance <= 100; distance += 10) {
            Experiment experiment;
            experiment.Run(distance, rate, manager);
            // Assuming dataset is stored in experiment and can be accessed
            // For simplicity, here we assume that the dataset is global or handled externally
            // This example omits saving to file as part of the simulation
        }

        std::ofstream plotFile(plot.GetOutputFileNameWithoutExtension() + ".plt");
        plot.GenerateOutput(plotFile);
        plotFile.close();
    }

    return 0;
}