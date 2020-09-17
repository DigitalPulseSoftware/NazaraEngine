#include <ShaderNode/DataModels/OutputValue.hpp>
#include <Nazara/Shader/ShaderBuilder.hpp>
#include <ShaderNode/ShaderGraph.hpp>
#include <ShaderNode/DataTypes/BoolData.hpp>
#include <ShaderNode/DataTypes/FloatData.hpp>
#include <ShaderNode/DataTypes/Matrix4Data.hpp>
#include <ShaderNode/DataTypes/VecData.hpp>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>

OutputValue::OutputValue(ShaderGraph& graph) :
ShaderNode(graph)
{
	m_onOutputListUpdateSlot.Connect(GetGraph().OnOutputListUpdate, [&](ShaderGraph*) { OnOutputListUpdate(); });
	m_onOutputUpdateSlot.Connect(GetGraph().OnOutputUpdate, [&](ShaderGraph*, std::size_t inputIndex)
	{
		if (m_currentOutputIndex == inputIndex)
			UpdatePreview();
	});

	if (graph.GetOutputCount() > 0)
	{
		m_currentOutputIndex = 0;
		UpdateOutputText();
	}

	EnablePreview();
	SetPreviewSize({ 128, 128 });
	DisableCustomVariableName();
}

void OutputValue::BuildNodeEdition(QFormLayout* layout)
{
	ShaderNode::BuildNodeEdition(layout);

	QComboBox* outputSelection = new QComboBox;
	for (const auto& outputEntry : GetGraph().GetOutputs())
		outputSelection->addItem(QString::fromStdString(outputEntry.name));

	if (m_currentOutputIndex)
		outputSelection->setCurrentIndex(int(*m_currentOutputIndex));
	
	connect(outputSelection, qOverload<int>(&QComboBox::currentIndexChanged), [&](int index)
	{
		if (index >= 0)
			m_currentOutputIndex = static_cast<std::size_t>(index);
		else
			m_currentOutputIndex.reset();

		UpdateOutputText();
		UpdatePreview();
	});

	layout->addRow(tr("Output"), outputSelection);
}

Nz::ShaderNodes::ExpressionPtr OutputValue::GetExpression(Nz::ShaderNodes::ExpressionPtr* expressions, std::size_t count) const
{
	using namespace Nz::ShaderBuilder;
	using namespace Nz::ShaderNodes;

	assert(count == 1);

	if (!m_currentOutputIndex)
		throw std::runtime_error("no output");

	const auto& outputEntry = GetGraph().GetOutput(*m_currentOutputIndex);
	auto output = Nz::ShaderBuilder::Identifier(Nz::ShaderBuilder::Output(outputEntry.name, ShaderGraph::ToShaderExpressionType(outputEntry.type)));

	return Nz::ShaderBuilder::Assign(std::move(output), *expressions);
}

QtNodes::NodeDataType OutputValue::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const
{
	assert(portType == QtNodes::PortType::In);
	assert(portIndex == 0);

	if (!m_currentOutputIndex)
		return VecData::Type();

	const auto& outputEntry = GetGraph().GetOutput(*m_currentOutputIndex);
	return ShaderGraph::ToNodeDataType(outputEntry.type);
}

unsigned int OutputValue::nPorts(QtNodes::PortType portType) const
{
	switch (portType)
	{
		case QtNodes::PortType::In: return 1;
		case QtNodes::PortType::Out: return 0;
	}

	return 0;
}

std::shared_ptr<QtNodes::NodeData> OutputValue::outData(QtNodes::PortIndex /*port*/)
{
	return {};
}

void OutputValue::setInData(std::shared_ptr<QtNodes::NodeData> value, int index)
{
	if (!m_currentOutputIndex)
		return;

	assert(index == 0);
	m_input = std::move(value);

	UpdatePreview();
}

QtNodes::NodeValidationState OutputValue::validationState() const
{
	if (!m_currentOutputIndex || !m_input)
		return QtNodes::NodeValidationState::Error;

	const auto& outputEntry = GetGraph().GetOutput(*m_currentOutputIndex);
	switch (outputEntry.type)
	{
		case PrimitiveType::Bool:
		case PrimitiveType::Float1:
		case PrimitiveType::Mat4x4:
			break;

		case PrimitiveType::Float2:
		case PrimitiveType::Float3:
		case PrimitiveType::Float4:
		{
			assert(dynamic_cast<VecData*>(m_input.get()) != nullptr);
			const VecData& vec = static_cast<const VecData&>(*m_input);
			if (GetComponentCount(outputEntry.type) != vec.componentCount)
				return QtNodes::NodeValidationState::Error;
		}
	}

	return QtNodes::NodeValidationState::Valid;
}

QString OutputValue::validationMessage() const
{
	if (!m_currentOutputIndex)
		return "No output selected";

	if (!m_input)
		return "Missing input";

	const auto& outputEntry = GetGraph().GetOutput(*m_currentOutputIndex);
	switch (outputEntry.type)
	{
		case PrimitiveType::Bool:
		case PrimitiveType::Float1:
		case PrimitiveType::Mat4x4:
			break;

		case PrimitiveType::Float2:
		case PrimitiveType::Float3:
		case PrimitiveType::Float4:
		{
			assert(dynamic_cast<VecData*>(m_input.get()) != nullptr);
			const VecData& vec = static_cast<const VecData&>(*m_input);

			std::size_t outputComponentCount = GetComponentCount(outputEntry.type);

			if (outputComponentCount != vec.componentCount)
				return "Incompatible component count (expected " + QString::number(outputComponentCount) + ", got " + QString::number(vec.componentCount) + ")";
		}
	}

	return QString();
}

bool OutputValue::ComputePreview(QPixmap& pixmap)
{
	if (!m_input)
		return false;

	const auto& outputEntry = GetGraph().GetOutput(*m_currentOutputIndex);
	switch (outputEntry.type)
	{
		case PrimitiveType::Bool:
		{
			assert(dynamic_cast<BoolData*>(m_input.get()) != nullptr);
			const BoolData& data = static_cast<const BoolData&>(*m_input);

			pixmap = QPixmap::fromImage(data.preview.GenerateImage());
			return true;
		}

		case PrimitiveType::Float1:
		{
			assert(dynamic_cast<FloatData*>(m_input.get()) != nullptr);
			const FloatData& data = static_cast<const FloatData&>(*m_input);

			pixmap = QPixmap::fromImage(data.preview.GenerateImage());
			return true;
		}

		case PrimitiveType::Mat4x4:
		{
			//TODO
			/*assert(dynamic_cast<Matrix4Data*>(m_input.get()) != nullptr);
			const Matrix4Data& data = static_cast<const Matrix4Data&>(*m_input);*/

			return false;
		}

		case PrimitiveType::Float2:
		case PrimitiveType::Float3:
		case PrimitiveType::Float4:
		{
			assert(dynamic_cast<VecData*>(m_input.get()) != nullptr);
			const VecData& data = static_cast<const VecData&>(*m_input);

			pixmap = QPixmap::fromImage(data.preview.GenerateImage());
			return true;
		}
	}

	return false;
}

void OutputValue::OnOutputListUpdate()
{
	m_currentOutputIndex.reset();

	std::size_t inputIndex = 0;
	for (const auto& inputEntry : GetGraph().GetOutputs())
	{
		if (inputEntry.name == m_currentOutputText)
		{
			m_currentOutputIndex = inputIndex;
			break;
		}

		inputIndex++;
	}
}

void OutputValue::UpdateOutputText()
{
	if (m_currentOutputIndex)
	{
		auto& output = GetGraph().GetOutput(*m_currentOutputIndex);
		m_currentOutputText = output.name;
	}
	else
		m_currentOutputText.clear();
}

void OutputValue::restore(const QJsonObject& data)
{
	m_currentOutputText = data["output"].toString().toStdString();
	OnOutputListUpdate();

	ShaderNode::restore(data);
}

QJsonObject OutputValue::save() const
{
	QJsonObject data = ShaderNode::save();
	data["output"] = QString::fromStdString(m_currentOutputText);

	return data;
}