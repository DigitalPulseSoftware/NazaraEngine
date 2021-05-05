// Copyright (C) 2020 Jérôme Leclercq
// This file is part of the "Nazara Engine - Shader generator"
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef NAZARA_SHADERASTOPTIMISER_HPP
#define NAZARA_SHADERASTOPTIMISER_HPP

#include <Nazara/Prerequisites.hpp>
#include <Nazara/Shader/Config.hpp>
#include <Nazara/Shader/Ast/AstCloner.hpp>
#include <optional>
#include <vector>

namespace Nz::ShaderAst
{
	class NAZARA_SHADER_API AstOptimizer : public AstCloner
	{
		public:
			AstOptimizer() = default;
			AstOptimizer(const AstOptimizer&) = delete;
			AstOptimizer(AstOptimizer&&) = delete;
			~AstOptimizer() = default;

			StatementPtr Optimise(const StatementPtr& statement);
			StatementPtr Optimise(const StatementPtr& statement, UInt64 enabledConditions);

			AstOptimizer& operator=(const AstOptimizer&) = delete;
			AstOptimizer& operator=(AstOptimizer&&) = delete;

		protected:
			ExpressionPtr Clone(BinaryExpression& node) override;
			ExpressionPtr Clone(ConditionalExpression& node) override;
			StatementPtr Clone(BranchStatement& node) override;
			StatementPtr Clone(ConditionalStatement& node) override;

			template<BinaryType Type> ExpressionPtr PropagateConstant(std::unique_ptr<ConstantExpression>&& lhs, std::unique_ptr<ConstantExpression>&& rhs);

		private:
			std::optional<UInt64> m_enabledOptions;
	};
}

#include <Nazara/Shader/Ast/AstOptimizer.inl>

#endif