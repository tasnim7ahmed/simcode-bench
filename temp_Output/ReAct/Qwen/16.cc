#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/onoff-application.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment");

class Experiment {
public:
    Experiment();
    ~Experiment();

    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
    void MoveNode(Ptr<Node> node, Vector newPosition, double timeSeconds);
    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReception(Ptr<Node> node);

    void Run(std::string rateManagerType, std::string dataRate, std::string experimentName);

private:
    Gnuplot2dDataset m_throughputDataset;
};

Experiment::Experiment() {
    m_throughputDataset = Gnuplot2dDataset(experimentName);
}

Experiment::~Experiment() {}

void Experiment::SetPosition(Ptr<Node> node, Vector position) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    if (mobility) {
        mobility->SetPosition(position);
    }
}

Vector Experiment::GetPosition(Ptr<Node> node) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    if (mobility) {
        return mobility->GetPosition();
    }
    return Vector();
}

void Experiment::MoveNode(Ptr<Node> node, Vector newPosition, double timeSeconds) {
    Simulator::Schedule(Seconds(timeSeconds), &MobilityModel::SetPosition, node->GetObject<MobilityModel>(), newPosition);
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
        NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
}

void Experiment::SetupPacketReception(Ptr<Node> node) {
    PacketSocketAddress address;
    address.SetSingleDevice(node->GetDevice(0)->GetIfIndex());
    address.SetPhysicalAddress(node->GetDevice(0)->GetAddress());
    address.SetProtocol(1);

    Ptr<Socket> recvSocket = Socket::CreateSocket(node, address.GetType());
    recvSocket->Bind(address);
    recvSocket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::Run(std::string rateManagerType, std::string dataRate, std::string experimentName) {
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager(rateManagerType, "DataMode", StringValue(dataRate),
                                 "ControlMode", StringValue(dataRate));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set initial positions
    SetPosition(nodes.Get(0), Vector(0.0, 0.0, 0.0));
    SetPosition(nodes.Get(1), Vector(10.0, 0.0, 0.0));

    // Setup packet reception on the second node
    SetupPacketReception(nodes.Get(1));

    // Configure OnOff application to send packets
    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps;
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), 8080));
    onoff.SetAttribute("Remote", remoteAddress);
    apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Schedule node movement
    MoveNode(nodes.Get(0), Vector(5.0, 0.0, 0.0), 5.0);
    MoveNode(nodes.Get(1), Vector(15.0, 0.0, 0.0), 5.0);

    // Logging throughput
    Config::Connect("/NodeList/1/ApplicationList/0/$ns3::OnOffApplication/Tx", MakeCallback(
        [this, experimentName](Ptr<const Packet> p) {
            static uint64_t totalBytes = 0;
            totalBytes += p->GetSize();
            double throughput = static_cast<double>(totalBytes * 8) / (Simulator::Now().GetSeconds() * 1000); // kbps
            m_throughputDataset.Add(Simulator::Now().GetSeconds(), throughput);
        }));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    std::string rateManagerType = "ns3::ConstantRateWifiManager";
    std::string dataRate = "HtMcs7"; // Default rate

    CommandLine cmd(__FILE__);
    cmd.AddValue("rateManagerType", "Type of rate manager (e.g., ConstantRateWifiManager)", rateManagerType);
    cmd.AddValue("dataRate", "Data rate (e.g., HtMcs7, OfdmRate24Mbps)", dataRate);
    cmd.Parse(argc, argv);

    // Create and run experiments for different configurations
    std::vector<std::pair<std::string, std::string>> experiments = {
        {"ns3::ConstantRateWifiManager", "OfdmRate6Mbps"},
        {"ns3::ConstantRateWifiManager", "OfdmRate24Mbps"},
        {"ns3::AarfcdWifiManager", ""},
        {"ns3::AmrrWifiManager", ""}
    };

    for (auto& exp : experiments) {
        std::string expName = exp.first.substr(exp.first.find_last_of("::") + 1) + "-" + exp.second;
        Experiment experiment;
        experiment.Run(exp.first, exp.second, expName);
    }

    // Generate gnuplot output
    Gnuplot plot("throughput-graphs.png");
    plot.SetTitle("Throughput Comparison");
    plot.SetTerminal("png");
    plot.SetLegend("Time (seconds)", "Throughput (kbps)");

    // Add datasets here from all experiments...

    std::ofstream plotFile("throughput.plt");
    plot.GenerateOutput(plotFile);

    return 0;
}